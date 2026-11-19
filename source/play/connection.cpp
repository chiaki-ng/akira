#include "play/connection.hpp"
#include "log/log.hpp"

#include <algorithm>
#include <chrono>
#include <ranges>
#include <utility>

#include <borealis.hpp>

#include "cloud/cloud_launch.hpp"
#include "play/chiaki_core.hpp"
#include "consoles/host.hpp"
#include "settings/settings_manager.hpp"
#include "network/wireguard_manager.hpp"
#include "play/sample_cadence.hpp"
#include "play/input_manager.hpp"
#include "play/session.hpp"
#include "platform/video/provider.hpp"
#include "platform/factory.hpp"

namespace akira::stream {

namespace play = libchiaking::play;
namespace platform = libchiaking::platform;

Connection::Connection(Session& media, std::weak_ptr<ConnectionListener> listener, bool ps5)
    : media(media)
    , listener(std::move(listener))
    , ps5(ps5)
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

auto Connection::preferencesFor(Host& host, SettingsManager& settings) -> std::optional<play::Preferences>
{
    const bool vpn = !host.isRemote() && WireGuardManager::instance().isConnected();
    uint32_t height = static_cast<uint32_t>(SettingsManager::resolutionToInt(
        vpn ? settings.getVpnVideoResolution() : settings.getVideoResolution(&host)));
    const auto fps = static_cast<uint32_t>(SettingsManager::fpsToInt(
        vpn ? settings.getVpnVideoFPS() : settings.getVideoFPS(&host)));
    const auto bitrate = static_cast<uint32_t>(std::clamp(
        vpn ? settings.getVpnVideoBitrate() : settings.getVideoBitrate(&host),
        static_cast<int>(play::PictureRequest::minimum_bitrate_kbps),
        static_cast<int>(play::PictureRequest::maximum_bitrate_kbps)));
    const auto codec = host.isPS5() && akira::platform::PlatformFactory::instance().supplies(akira::platform::PlatformCapability::hevc)
        ? libchiaking::media::VideoCodec::h265 : libchiaking::media::VideoCodec::h264;
    if (codec == libchiaking::media::VideoCodec::h264)
        height = std::min(height, play::PictureRequest::maximum_h264_height);

    const auto picture = play::PictureRequest::create(height, fps, bitrate, codec);
    if (!picture)
    {
        akira::log::error("Stream preferences refused: {}p {} fps {} kbps", height, fps, bitrate);
        return std::nullopt;
    }

    const auto target = host.isPS5() ? TakionTarget::PS5 : TakionTarget::PS4;
    const auto takionVersion = host.getPinnedTakionVersion().value_or(
        static_cast<uint32_t>(std::max(settings.getTakionVersion(target), 0)));
    if (host.getPinnedTakionVersion())
        akira::log::info("{} is pinned to Takion v{}", host.getHostName(), takionVersion);
    auto preferences = tuned(*picture, takionVersion, settings);
    if (preferences && vpn)
    {
        if (auto network = host.homeNetwork(); network.network())
            preferences->home = std::move(network);
        else
            akira::log::error("Stream over WireGuard has no tunnel network; using the local network");
    }
    return preferences;
}

auto Connection::cloudPreferencesFor(const cloud::CloudLaunch& launch, SettingsManager& settings)
    -> std::optional<play::Preferences>
{
    const bool ps5Cloud = launch.ps5();
    const auto codec = ps5Cloud && akira::platform::PlatformFactory::instance().supplies(akira::platform::PlatformCapability::hevc)
        ? libchiaking::media::VideoCodec::h265 : libchiaking::media::VideoCodec::h264;
    auto height = static_cast<uint32_t>(settings.getCloudVideoResolution(ps5Cloud));
    if (codec == libchiaking::media::VideoCodec::h264)
        height = std::min(height, play::PictureRequest::maximum_h264_height);
    const auto bitrate = static_cast<uint32_t>(std::clamp(settings.getCloudVideoBitrate(ps5Cloud),
        static_cast<int>(play::PictureRequest::minimum_bitrate_kbps),
        static_cast<int>(play::PictureRequest::maximum_bitrate_kbps)));
    const auto picture = play::PictureRequest::create(height, 60, bitrate, codec);
    if (!picture)
    {
        akira::log::error("Cloud stream preferences refused: {}p {} kbps", height, bitrate);
        return std::nullopt;
    }
    return tuned(*picture,
        static_cast<uint32_t>(std::max(settings.getTakionVersion(ps5Cloud ? TakionTarget::PS5Cloud : TakionTarget::PSNow), 0)),
        settings);
}

auto Connection::tuned(play::PictureRequest picture, std::uint32_t takionVersion, SettingsManager& settings)
    -> std::optional<play::Preferences>
{
    play::DeveloperTuning::Facts facts;
    if (takionVersion > 0)
        facts.protocol_version = takionVersion;
    facts.packet_loss_max = settings.getPacketLossMax();
    facts.keyframe_on_lost_frame = settings.getRequestIdrOnFecFailure();
    facts.nat_probe_sockets = static_cast<std::size_t>(std::clamp(settings.getPortGuessingSocks(), 0,
        static_cast<int>(play::DeveloperTuning::maximum_nat_probe_sockets)));
    const auto developer = play::DeveloperTuning::create(facts);
    const auto internet = play::InternetOptions::create(settings.getPortGuessing(),
        static_cast<std::size_t>(std::max(settings.getPortGuessingCount(), 0)));
    if (!developer || !internet)
    {
        akira::log::error("Stream tuning refused: packet loss {} sockets {}", facts.packet_loss_max, facts.nat_probe_sockets);
        return std::nullopt;
    }

    play::Preferences preferences;
    preferences.picture = picture;
    preferences.sound = play::SoundRequest::create(play::SoundRequest::stereo)
                            .and_then([](const play::SoundRequest& sound) { return sound.capped_at(play::SoundRequest::stereo); })
                            .transform([](const play::SoundRequest& sound) { return sound.with_audio_profiles(false); })
                            .value_or(play::SoundRequest{});
    preferences.developer = *developer;
    preferences.internet = *internet;
    return preferences;
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
    auto started = platform::Thread::start(ChiakiCore::current().threading(), "stream-input",
        platform::ThreadRole::input,
        [this, starter = std::move(starter)](platform::StopToken stop) {
            auto opened = starter(*this, *this, stop);
            if (!opened)
            {
                akira::log::error("Stream start refused: kind {}", static_cast<int>(opened.error().kind()));
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
        akira::log::error("Stream input thread did not start");
        return false;
    }
    worker = std::move(started).value();
    return true;
}

void Connection::feed(play::Session& running, const platform::StopToken& stop)
{
    auto& core = ChiakiCore::current();
    platform::Sleeper sleeper(core.threading(), core.clock());
    const auto period = std::chrono::duration_cast<std::chrono::microseconds>(akira::input::InputPollPeriod);
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
        akira::log::info("[core] {}", entry->line());
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
    brls::sync([token = aliveToken, self = this, to = listener, work = std::move(work)]() {
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
        akira::log::error("Login PIN refused: {}", static_cast<int>(submitted.error()));
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
    if (!akira::platform::PlatformFactory::instance().supplies(akira::platform::PlatformCapability::couch_seats))
        return false;

    const auto seat = libchiaking::controllers::SeatNumber::create(slot);
    const auto running = session();
    if (!seat || seat->is_first() || !running)
        return false;

    auto* settings = SettingsManager::getInstance();
    std::optional<libchiaking::AccountId> account;
    std::string label;
    if (ps5)
    {
        const Profile* profile = profileId ? settings->findProfile(*profileId) : nullptr;
        if (!profile)
            return false;
        account = libchiaking::AccountId::from_base64(profile->accountId);
        if (!account)
        {
            akira::log::error("Couch: profile {} has an invalid account id", *profileId);
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
        akira::log::error("Couch: join for player {} refused: {}", slot + 1, static_cast<int>(added.error()));
        entry.phase = SeatPhase::Failed;
        entry.refusal = play::SeatRefusal::not_sent(*seat, play::EndReason::Cause::invalid_state);
        return false;
    }
    akira::log::info("Couch: requesting player {}", slot + 1);
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
        akira::log::info("Couch: closing pad for player {}", slot + 1);
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
    if (!akira::platform::PlatformFactory::instance().supplies(akira::platform::PlatformCapability::couch_seats) || !session())
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
    auto* settings = SettingsManager::getInstance();
    if (!ps5 || !settings || slot == 0 || slot >= seatCount)
        return choices;

    const int64_t activeProfileId = settings->getActiveProfileId();
    for (const Profile& profile : settings->getProfiles())
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
    akira::platform::PlatformFactory::instance().pads().routeSeatFeedback(slot, feedback);
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
        akira::log::info("Couch: console dropped player {}", slot + 1);
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
            akira::log::error("Couch: no free seat on the console");
            return;
        }
        const uint8_t slot = refusal.seat()->index();
        akira::log::error("Couch: join for player {} refused, kind {}", slot + 1, static_cast<int>(refusal.kind()));
        if (self.seats[slot].phase == SeatPhase::Empty)
            return;
        self.seats[slot].phase = SeatPhase::Failed;
        self.seats[slot].refusal = refusal;
        to.onSeatRefused(slot, refusal);
    });
}

}
