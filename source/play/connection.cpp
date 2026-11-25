#include "play/connection.hpp"
#include "log/log.hpp"

#include <algorithm>
#include <chrono>
#include <ranges>
#include <utility>

#include "cloud/cloud_launch.hpp"
#include "play/chiaki_core.hpp"
#include "consoles/host.hpp"
#include "accounts/account_settings.hpp"
#include "play/sample_cadence.hpp"
#include "play/input_manager.hpp"
#include "play/session.hpp"
#include "platform/video/provider.hpp"
#include "platform/factory.hpp"

namespace akira::play {

namespace play = libchiaking::play;
namespace platform = libchiaking::platform;

Connection::Connection(Session& media, std::weak_ptr<ConnectionListener> listener, bool ps5,
                       akira::platform::PlatformFactory& platform, const accounts::AccountSettings& accounts,
                       app::UiDispatcher& dispatcher, ChiakiCore& core)
    : media(media)
    , listener(std::move(listener))
    , ps5(ps5)
    , platform(platform)
    , accounts(accounts)
    , dispatcher(dispatcher)
    , core(core)
{
}

Connection::~Connection()
{
    alive.reset();
    worker = platform::Thread{};

    std::shared_ptr<play::Session> ending;
    {
        std::lock_guard lock(sessionMutex);
        ending = std::move(active);
    }
    if (ending)
    {
        if (ending->state() != play::SessionState::leaving)
            static_cast<void>(ending->leave());
        static_cast<void>(ending->wait_until_ended());
        drainJournal(*ending);
    }
    media.detach();
    ending.reset();
}

auto Connection::consoleStarter(std::shared_ptr<libchiaking::consoles::Console> console,
                                std::shared_ptr<libchiaking::users::PsUser> player,
                                play::Preferences preferences,
                                libchiaking::consoles::Route route) -> Starter
{
    return [console = std::move(console), player = std::move(player), preferences = std::move(preferences), route](
               libchiaking::media::MediaSink& sink, play::SessionEvents& events, const platform::StopToken& stop) -> Started {
        return console->start(player, preferences, sink, events, route, stop);
    };
}

bool Connection::open(Starter starter)
{
    auto started = platform::Thread::start(core.threading(), "stream-input",
        platform::ThreadRole::input,
        [this, starter = std::move(starter)](platform::StopToken stop) {
            auto opened = starter(*this, *this, stop);
            if (!opened)
            {
                log::error("Stream start refused: kind {}", static_cast<int>(opened.error().kind()));
                post([refusal = opened.error()](Connection&, ConnectionListener& to) { to.onRefused(refusal); });
                return;
            }
            auto running = std::move(opened).value();
            {
                std::lock_guard lock(sessionMutex);
                active = running;
            }
            media.attach(running);
            media.startStreamTimer();
            feed(*running, stop);
        });
    if (!started)
    {
        log::error("Stream input thread did not start");
        return false;
    }
    worker = std::move(started).value();
    return true;
}

void Connection::feed(play::Session& running, const platform::StopToken& stop)
{
    platform::Sleeper sleeper(core.threading(), core.clock());
    const auto period = std::chrono::duration_cast<std::chrono::microseconds>(InputPollPeriod);
    auto next = core.clock().now_microseconds();
    auto journalDue = next;

    for (auto& input : inputs)
        input.set_idle();

    while (!stop.stop_requested() && running.state() != play::SessionState::ended)
    {
        sendInput(running);
        next += static_cast<uint64_t>(period.count());
        const auto now = core.clock().now_microseconds();
        if (now >= journalDue)
        {
            drainJournal(running);
            journalDue = now + static_cast<uint64_t>(journalDrainPeriod.count());
        }
        if (now >= next + static_cast<uint64_t>(period.count()))
            next = now;
        if (next > now && !sleeper.sleep_for(std::chrono::microseconds(next - now), stop))
            break;
    }
    drainJournal(running);
}

void Connection::drainJournal(const play::Session& running)
{
    const auto& journal = running.journal();
    while (const auto entry = journal->read())
        log::info("[core] {}", entry->line());
}

void Connection::sendInput(play::Session& running)
{
    if (inputSuppressed.load(std::memory_order_acquire))
    {
        fingerIdTouchId.clear();
        for (auto& input : inputs)
            input.set_idle();
    }
    else
    {
        media.UpdateControllerState(inputs[0], &fingerIdTouchId);
        uint8_t count = 1;
        media.UpdateSecondaryPads(inputs, count);
        couchPadCount = count;
    }

    static_cast<void>(running.controllers().send(libchiaking::controllers::SeatNumber::first(), inputs[0].state()));
    for (uint8_t slot = 1; slot < couchPadCount && slot < seatCount; slot++)
    {
        const auto seat = libchiaking::controllers::SeatNumber::create(slot);
        if (seat && running.seats().seat(*seat).state() == play::SeatState::playing)
            static_cast<void>(running.controllers().send(*seat, inputs[slot].state()));
    }
}

auto Connection::session() const -> std::shared_ptr<play::Session>
{
    std::lock_guard lock(sessionMutex);
    return active;
}

void Connection::post(std::function<void(Connection&, ConnectionListener&)> work)
{
    dispatcher.post([token = aliveToken, self = this, to = listener, work = std::move(work)]() {
        if (token.expired())
            return;
        if (auto target = to.lock())
            work(*self, *target);
    });
}

auto Connection::state() const -> std::optional<play::SessionState>
{
    const auto running = session();
    if (!running)
        return std::nullopt;
    return running->state();
}

bool Connection::submitLoginPin(std::string_view pin)
{
    const auto running = session();
    auto code = libchiaking::AccessCode::parse(pin);
    if (!running || !code)
        return false;
    const auto submitted = running->login_pin().submit(std::move(*code));
    if (!submitted)
        log::error("Login PIN refused: {}", static_cast<int>(submitted.error()));
    return submitted.has_value();
}

void Connection::leave(bool restConsole)
{
    if (const auto running = session())
        static_cast<void>(running->leave(restConsole ? play::Leave::rest_console : play::Leave::disconnect));
}

void Connection::setInputSuppressed(bool suppressed)
{
    inputSuppressed.store(suppressed, std::memory_order_release);
}

bool Connection::addPlayer(uint8_t slot, std::optional<int64_t> profileId)
{
    if (!this->platform.supplies(akira::platform::PlatformCapability::couch_seats))
        return false;

    const auto seat = libchiaking::controllers::SeatNumber::create(slot);
    const auto running = session();
    if (!seat || seat->is_first() || !running)
        return false;

    std::optional<libchiaking::AccountId> account;
    std::string label;
    if (ps5)
    {
        const accounts::Profile* profile = profileId ? this->accounts.findProfile(*profileId) : nullptr;
        if (!profile)
            return false;
        account = libchiaking::AccountId::from_base64(profile->accountId);
        if (!account)
        {
            log::error("Couch: profile {} has an invalid account id", *profileId);
            return false;
        }
        label = profile->label();
    }

    const auto kind = ps5 ? libchiaking::controllers::ControllerKind::console_default : libchiaking::controllers::ControllerKind::dualshock4;
    const auto added = running->seats().add_player(*seat, kind, account);
    CouchSeat& entry = seats[slot];
    entry = CouchSeat{};
    entry.accountLabel = label;
    entry.profileId = profileId.value_or(0);
    if (!added)
    {
        log::error("Couch: join for player {} refused: {}", slot + 1, static_cast<int>(added.error()));
        entry.phase = SeatPhase::Failed;
        entry.refusal = play::SeatRefusal::not_sent(*seat, play::EndReason::Cause::invalid_state);
        return false;
    }
    log::info("Couch: requesting player {}", slot + 1);
    entry.phase = SeatPhase::Joining;
    return true;
}

void Connection::removePlayer(uint8_t slot)
{
    const auto seat = libchiaking::controllers::SeatNumber::create(slot);
    if (!seat || seat->is_first())
        return;
    seats[slot] = CouchSeat{};
    if (const auto running = session())
    {
        log::info("Couch: closing pad for player {}", slot + 1);
        static_cast<void>(running->seats().remove_player(*seat));
    }
}

void Connection::sendSeatPasscode(uint8_t slot, std::string_view passcode)
{
    const auto seat = libchiaking::controllers::SeatNumber::create(slot);
    const auto running = session();
    auto code = libchiaking::AccessCode::parse(passcode);
    if (!seat || !running || !code)
        return;
    seats[slot].phase = SeatPhase::Joining;
    static_cast<void>(running->seats().enter_passcode(*seat, std::move(*code)));
}

auto Connection::couchSeat(uint8_t slot) const -> const CouchSeat&
{
    static const CouchSeat empty;
    return slot < seatCount ? seats[slot] : empty;
}

bool Connection::couchSlotAvailable() const
{
    if (!this->platform.supplies(akira::platform::PlatformCapability::couch_seats) || !session())
        return false;
    for (uint8_t slot = 1; slot < seatCount; slot++)
    {
        if (seats[slot].phase == SeatPhase::Empty)
            return true;
    }
    return false;
}

bool Connection::seatPlaying(uint8_t slot) const
{
    const auto seat = libchiaking::controllers::SeatNumber::create(slot);
    const auto running = session();
    return seat && running && running->seats().seat(*seat).state() == play::SeatState::playing;
}

auto Connection::couchProfileChoices(uint8_t slot) const -> std::vector<CouchProfileChoice>
{
    std::vector<CouchProfileChoice> choices;
    if (!ps5 || slot == 0 || slot >= seatCount)
        return choices;

    const int64_t activeProfileId = this->accounts.activeProfileId();
    for (const accounts::Profile& profile : this->accounts.profiles())
    {
        if (profile.id == activeProfileId || profile.accountId.empty())
            continue;
        const bool assignedElsewhere = std::ranges::any_of(std::views::iota(uint8_t{1}, seatCount),
            [&](uint8_t other) { return other != slot && seats[other].phase != SeatPhase::Empty && seats[other].profileId == profile.id; });
        if (!assignedElsewhere)
            choices.push_back({profile.id, profile.label(), profile.accountId, profile.avatarUrl});
    }
    return choices;
}

void Connection::on_stream_description(const libchiaking::media::StreamDescription& description)
{
    media.describeStream(description);
}

auto Connection::on_video(const libchiaking::media::VideoUnit& unit) -> libchiaking::media::VideoResult
{
    return media.submitVideo(unit);
}

void Connection::on_video_gap()
{
}

auto Connection::on_audio(const libchiaking::media::AudioUnit& unit) -> libchiaking::media::AudioResult
{
    return media.submitAudio(unit);
}

auto Connection::decoder_name() const -> std::string_view
{
    return media.video().decoderName();
}

void Connection::on_feedback(const libchiaking::controllers::ControllerFeedback& feedback)
{
    if (feedback.seat.is_added())
    {
        routeCouchFeedback(feedback);
        return;
    }

    if (const auto* haptics = feedback.get<libchiaking::controllers::HapticAudio>())
    {
        if (haptics->playable())
            media.HapticCB(haptics->pcm);
    }
    else if (const auto* rumble = feedback.get<libchiaking::controllers::Rumble>())
    {
        media.SetRumble(rumble->left, rumble->right);
    }
    else if (feedback.get<libchiaking::controllers::MotionReset>())
    {
        if (auto* input = media.getInputManager())
            input->resetMotionControls();
    }
    else
    {
        media.ApplyPadFeedback(feedback);
    }
}

void Connection::routeCouchFeedback(const libchiaking::controllers::ControllerFeedback& feedback)
{
    const uint8_t slot = feedback.seat.index();
    if (!seatPlaying(slot))
        return;
    this->platform.pads().routeSeatFeedback(slot, feedback);
}

void Connection::on_state(play::SessionState state)
{
    if (state == play::SessionState::playing)
        post([](Connection&, ConnectionListener& to) { to.onPlaying(); });
}

void Connection::on_ended(const play::Outcome& outcome)
{
    post([outcome](Connection&, ConnectionListener& to) { to.onEnded(outcome); });
}

void Connection::on_connect_progress(const play::ConnectProgress& progress)
{
    post([progress](Connection&, ConnectionListener& to) { to.onConnectProgress(progress); });
}

void Connection::on_launch_progress(const play::LaunchProgress& progress)
{
    post([progress](Connection&, ConnectionListener& to) { to.onLaunchProgress(progress); });
}

void Connection::on_login_pin_needed(bool previousWasWrong)
{
    post([previousWasWrong](Connection&, ConnectionListener& to) { to.onLoginPinNeeded(previousWasWrong); });
}

void Connection::on_seat_passcode_needed(libchiaking::controllers::SeatNumber seat, bool retry)
{
    post([slot = seat.index(), retry](Connection& self, ConnectionListener& to) {
        self.seats[slot].phase = SeatPhase::Passcode;
        to.onSeatPasscodeNeeded(slot, retry);
    });
}

void Connection::on_seat_joined(libchiaking::controllers::SeatNumber seat, libchiaking::controllers::SeatLight)
{
    post([slot = seat.index()](Connection& self, ConnectionListener&) {
        if (self.seats[slot].phase == SeatPhase::Empty)
            return;
        self.seats[slot].phase = SeatPhase::Playing;
        self.seats[slot].refusal.reset();
    });
}

void Connection::on_seat_left(libchiaking::controllers::SeatNumber seat)
{
    post([slot = seat.index()](Connection& self, ConnectionListener& to) {
        if (self.seats[slot].phase == SeatPhase::Empty)
            return;
        log::info("Couch: console dropped player {}", slot + 1);
        self.seats[slot].phase = SeatPhase::Failed;
        self.seats[slot].dropped = true;
        to.onSeatDropped(slot);
    });
}

void Connection::on_seat_refused(const play::SeatRefusal& refusal)
{
    post([refusal](Connection& self, ConnectionListener& to) {
        if (!refusal.seat())
        {
            log::error("Couch: no free seat on the console");
            return;
        }
        const uint8_t slot = refusal.seat()->index();
        log::error("Couch: join for player {} refused, kind {}", slot + 1, static_cast<int>(refusal.kind()));
        if (self.seats[slot].phase == SeatPhase::Empty)
            return;
        self.seats[slot].phase = SeatPhase::Failed;
        self.seats[slot].refusal = refusal;
        to.onSeatRefused(slot, refusal);
    });
}

}
