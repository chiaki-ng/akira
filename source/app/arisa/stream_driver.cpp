#include "app/arisa/stream_driver.hpp"

#include <arisa/composition/overlay_layer.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/overlay_behaviour.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "app/arisa/couch_bridge.hpp"
#include "app/arisa/shell_model.hpp"
#include "app/arisa/window.hpp"
#include "app/notifier.hpp"
#include "app/strings.hpp"
#include "consoles/host.hpp"
#include "consoles/host_catalog.hpp"
#include "log/log.hpp"
#include "network/network_settings.hpp"
#include "network/wireguard_manager.hpp"
#include "platform/factory.hpp"
#include "play/connect_failure.hpp"
#include "play/input_manager.hpp"
#include "play/lazy_session.hpp"
#include "play/session.hpp"
#include "play/session_ending.hpp"
#include "play/stream_launcher.hpp"
#include "play/stream_recovery.hpp"
#include "play/stream_route.hpp"
#include "play/stream_state.hpp"
#include "play/pad_choice.hpp"
#include "play/ui/connecting_screen.hpp"
#include "play/ui/couch_prompt.hpp"
#include "play/ui/pad_picker.hpp"
#include "play/ui/seat_passcode.hpp"
#include "play/ui/stream_dock.hpp"

namespace akira::app::arisa {

namespace composition = ::arisa::composition;
namespace provider = ::arisa::provider;
namespace runtime = ::arisa::runtime;

StreamDriver::StreamDriver(Services& services, runtime::Application& application, Window& window, ShellModel& model,
    Notifier& notifier, settings::ui::ControlOptions& controlOptions, play::ui::Mapping& mapping, Couch couch,
    std::function<void()> ended)
    : services(services)
    , application(application)
    , window(window)
    , model(model)
    , controlOptions(controlOptions)
    , mapping(mapping)
    , couch(couch)
    , notifier(notifier)
    , ended(std::move(ended))
{
}

StreamDriver::~StreamDriver()
{
    focusWatch.reset();
    if (connection)
        connection->leave(false);
    passcode.reset();
    prompt.reset();
    picker.reset();
    dock.reset();
    if (couch.table)
        couch.table->detach();
    connection.reset();
}

auto StreamDriver::modal(composition::Element content) -> runtime::OverlayHandle
{
    return application.root().overlays().show(composition::OverlayLayer::dialog, std::move(content),
        runtime::OverlayBehaviour{
            .input = composition::OverlayInput::modal,
            .alignment = composition::Alignment::center,
        });
}

auto StreamDriver::prepareController() -> bool
{
    if (controllerReady)
        return true;
    controllerReady = services.session.get().InitController();
    return controllerReady;
}

void StreamDriver::watchFocus()
{
    focusWatch = window.watchFocus([weak = weak_from_this()](bool focused) {
        if (const auto self = weak.lock())
            self->focusChanged(focused);
    });
}

void StreamDriver::start(consoles::Host& target)
{
    if (connection || waitingForPads || picker)
        return;
    if (!couch.table || !couch.model)
    {
        open(target, libchiaking::play::ConsoleHandover::fresh);
        return;
    }
    if (!prepareController())
    {
        fail(services.strings.text("akira/stream/failed_init_controller"), false);
        return;
    }
    pending = &target;
    padAttempt = 0;
    nextPadPoll = Clock::now();
    waitingForPads = true;
    log::info("[COUCH] waiting for the pad backend before {}", target.getHostName());
}

void StreamDriver::waitForPads()
{
    const Clock::time_point now = Clock::now();
    if (now < nextPadPoll)
        return;
    nextPadPoll = now + play::PadChoice::waitInterval;
    if (couch.table->padsSettled() || padAttempt >= play::PadChoice::waitAttempts)
    {
        log::info("[COUCH] pad choice after {}ms", padAttempt * play::PadChoice::waitInterval.count());
        waitingForPads = false;
        finishPadChoice();
        return;
    }
    ++padAttempt;
}

void StreamDriver::showPicker(play::ui::Couch::Picked picked)
{
    couch.model->askPad(couch.table->pads(), Clock::now(), std::move(picked));
    picker.emplace(modal(composition::Element(play::ui::PadPicker(*couch.model, services.strings))));
}

void StreamDriver::finishPadChoice()
{
    const std::vector<play::CouchTable::Pad> pads = couch.table->pads();
    if (!play::PadChoice::worthAsking(pads))
    {
        log::info("[COUCH] {} pad(s), nothing worth asking", pads.size());
        if (const std::optional<play::PadId> preferred = play::PadChoice::fallback(pads))
            couch.table->choosePrimary(*preferred);
        padChosen(play::ui::Couch::Pick::Default, std::nullopt);
        return;
    }
    log::info("[COUCH] asking between {} pads", pads.size());
    showPicker([weak = weak_from_this()](play::ui::Couch::Pick pick, std::optional<play::PadId> pad) {
        if (const auto self = weak.lock())
            self->padChosen(pick, pad);
    });
}

void StreamDriver::padChosen(play::ui::Couch::Pick pick, std::optional<play::PadId> pad)
{
    picker.reset();
    consoles::Host* target = pending;
    pending = nullptr;
    if (pick == play::ui::Couch::Pick::Cancelled)
    {
        log::info("[COUCH] pad choice abandoned before the stream started");
        services.session.get().FreeController();
        controllerReady = false;
        return;
    }
    if (pick == play::ui::Couch::Pick::Chosen && pad)
        couch.table->choosePrimary(*pad);
    if (target)
        open(*target, libchiaking::play::ConsoleHandover::fresh);
}

void StreamDriver::padArrived(play::PadId pad)
{
    if (!couch.table || !couch.model || !connection || !videoActive)
        return;
    if (!couch.table->slotFree())
    {
        log::info("[COUCH] pad {} arrived but no seat is free", static_cast<std::uint32_t>(pad));
        return;
    }
    if (prompt || dock || picker || passcode)
    {
        log::info("[COUCH] pad {} arrived while the UI is busy, not prompting", static_cast<std::uint32_t>(pad));
        return;
    }
    pauseForUi();
    couch.model->arrived([weak = weak_from_this()](play::ui::Couch::Arrival answer) {
        if (const auto self = weak.lock())
            self->arrivalAnswered(answer);
    });
    prompt.emplace(modal(composition::Element(play::ui::CouchPrompt(*couch.model, services.strings))));
    log::info("[COUCH] showing the controller arrival prompt");
}

void StreamDriver::arrivalAnswered(play::ui::Couch::Arrival answer)
{
    prompt.reset();
    if (!connection)
        return;
    switch (answer)
    {
        case play::ui::Couch::Arrival::AddPlayer:
            arrivalDock = true;
            openDock(play::StreamControls::Control::Players);
            couch.model->invite();
            return;
        case play::ui::Couch::Arrival::NotNow:
        {
            couch.table->resolveArrival();
            const std::vector<play::CouchTable::Pad> pads = couch.table->pads();
            const bool seated = std::ranges::any_of(pads, [](const play::CouchTable::Pad& shown) {
                return shown.seat.has_value() && *shown.seat != 0;
            });
            if (!seated && play::PadChoice::worthAsking(pads))
            {
                log::info("[COUCH] opening the primary pad picker after Not now");
                showPicker([weak = weak_from_this()](play::ui::Couch::Pick pick, std::optional<play::PadId> pad) {
                    const auto self = weak.lock();
                    if (!self)
                        return;
                    self->picker.reset();
                    if (pick == play::ui::Couch::Pick::Chosen && pad)
                        self->couch.table->choosePrimary(*pad);
                    self->resumeAfterUi();
                });
                return;
            }
            resumeAfterUi();
            return;
        }
        case play::ui::Couch::Arrival::Dismissed:
            couch.table->resolveArrival();
            resumeAfterUi();
            return;
    }
}

void StreamDriver::passcodeClosed()
{
    passcode.reset();
    if (pausedForPasscode)
        resumeAfterUi();
    pausedForPasscode = false;
}

void StreamDriver::pauseForUi()
{
    if (!connection)
        return;
    connection->setInputSuppressed(true);
    services.session.get().setVideoPaused(true);
    padsOwned.reset();
}

void StreamDriver::resumeAfterUi()
{
    if (!connection)
        return;
    services.session.get().setVideoPaused(false);
    connection->setInputSuppressed(false);
    if (videoActive && !dock)
        padsOwned.emplace(application.block_input());
}

auto StreamDriver::couchVisible() const -> bool
{
    return couch.model && (picker || prompt || passcode || dock);
}

void StreamDriver::open(consoles::Host& target, libchiaking::play::ConsoleHandover handover)
{
    if (connection)
        return;

    play::Session& session = services.session.get();
    const play::StreamRoute route(std::nullopt, target.isRemote(), services.wireGuard.isConnected(), target.isPS5());
    services.streamState.setProfile(route.profile());

    log::info("[ARISA] starting stream to {}", target.getHostName());
    if (!prepareController())
    {
        fail(services.strings.text("akira/stream/failed_init_controller"), false);
        return;
    }

    play::StreamLauncher launcher(session, services.platform, services.preferences, services.accounts,
        services.dispatcher, services.core);
    auto opened = launcher.open(target, nullptr, weak_from_this(), handover, route.context());
    if (!opened)
    {
        log::error("[ARISA] the stream could not start: failure {}", static_cast<int>(opened.error()));
        session.FreeController();
        controllerReady = false;
        session.FreeVideo();
        fail(services.strings.format("akira/connection/connect_failed_title",
                 services.strings.text(opened.error() == play::StreamLauncher::Failure::NotRegistered
                         ? "akira/connection/fail_regist"
                         : "akira/connection/fail_generic")),
            false);
        return;
    }

    connection = std::move(*opened);
    host = &target;
    lastConsole = target.getHostName();
    sessionStarted = false;
    intentionalDisconnect = false;
    controls = play::StreamControls();
    controls.offerSleep(!target.cloudLaunch());
    controls.showStats(session.getStatsOverlayMode());
    if (couch.table && couch.model)
    {
        controls.offerPlayers();
        controls.enablePlayers(!target.isCloud());
        couch.table->attach(connection, target.isPS5());
    }
    flow.restart();
    model.connecting(target.getHostName());
}

void StreamDriver::tick()
{
    if (waitingForPads)
        waitForPads();
    if (couchVisible())
        couch.model->tick(Clock::now());
    if (!connection)
        return;

    if (window.takeVideoStart())
        enterVideo();

    if (!videoActive)
    {
        model.stage(play::ui::ConnectingScreen::progressOf(flow.ring(), host ? host->getHostName() : lastConsole,
            services.strings));
        return;
    }

    if (!dock && !prompt && !passcode && !picker)
        followMenu();
    if (couch.table && couch.table->degradedOnce())
        notifier.notify(services.strings.text("akira/settings/analog_triggers_degraded"));

    if (!services.session.get().MainLoop())
    {
        log::warning("[ARISA] the session main loop stopped");
        connection->leave(false);
        stop();
    }
}

void StreamDriver::enterVideo()
{
    if (videoActive)
        return;
    videoActive = true;
    padsOwned.emplace(application.block_input());
    videoOwnsScreen.emplace(application.exclusive_frame(composition::ExclusiveFrame::Options{
        .keep = { composition::OverlayLayer::dialog, composition::OverlayLayer::toast },
    }));
    services.platform.display().streaming(true);
    model.streaming();
    log::info("[ARISA] video owns the screen; the main tree is exclusive");
}

void StreamDriver::followMenu()
{
    play::InputManager* input = services.session.get().getInputManager();
    const bool held = input != nullptr && input->menuHeld();
    if (menuHold.update(held, play::MenuHold::Clock::now()))
        openDock();
}

void StreamDriver::openDock(std::optional<play::StreamControls::Control> opening)
{
    play::Session& session = services.session.get();
    connection->setInputSuppressed(true);
    session.setVideoPaused(true);
    session.CleanUpHaptic();
    padsOwned.reset();
    if (couch.model)
        couch.model->refresh();
    const bool players = couch.model && host && !host->isCloud();
    dock.emplace(application.root().overlays().show(composition::OverlayLayer::dialog,
        play::ui::StreamDock(
            play::ui::StreamDock::Session{
                .console = host ? host->getHostName() : lastConsole,
                .controls = controls.controls(),
                .sleepOffered = controls.offersSleep(),
                .statsMode = session.getStatsOverlayMode(),
                .players = players,
                .opening = opening,
            },
            *this, controlOptions, mapping, couch.model, services.strings),
        runtime::OverlayBehaviour{
            .input = composition::OverlayInput::modal,
            .alignment = composition::Alignment::bottom,
        }));
    log::info("[ARISA] stream dock opened");
}

void StreamDriver::closeDock()
{
    mapping.cancel();
    if (couch.model)
        couch.model->close();
    if (arrivalDock && couch.table)
        couch.table->resolveArrival();
    arrivalDock = false;
    const bool wasOpen = dock.has_value();
    dock.reset();
    if (wasOpen && videoActive && connection)
        padsOwned.emplace(application.block_input());
}

void StreamDriver::resume()
{
    if (!connection)
        return;
    closeDock();
    services.session.get().setVideoPaused(false);
    connection->setInputSuppressed(false);
    log::info("[ARISA] stream resumed");
}

void StreamDriver::leave(bool rest)
{
    if (!connection)
        return;
    log::info("[ARISA] leaving the stream, rest={}", rest);
    intentionalDisconnect = true;
    connection->leave(rest);
    stop();
}

void StreamDriver::cancel()
{
    if (!connection)
        return;
    log::info("[ARISA] connection cancelled");
    intentionalDisconnect = true;
    connection->leave(false);
    stop();
}

void StreamDriver::reconnect()
{
    if (connection)
        return;
    const auto& entries = services.hosts.entries();
    const auto found = entries.find(lastConsole);
    if (found == entries.end() || !found->second)
        return;
    notifier.notify(services.strings.text("akira/stream/reconnecting"));
    open(*found->second, libchiaking::play::ConsoleHandover::just_left);
}

void StreamDriver::resetMotion()
{
    if (play::InputManager* input = services.session.get().getInputManager())
        input->resetMotionControls();
}

void StreamDriver::showStats(play::StatsOverlayMode mode)
{
    services.session.get().setStatsOverlayMode(mode);
    controls.showStats(mode);
}

auto StreamDriver::stats() -> play::StreamStats
{
    return services.session.get().getStreamStats();
}

auto StreamDriver::decoder() -> std::string
{
    play::Session& session = services.session.get();
    if (!session.getStreamStats().is_hardware_decoder)
        return "SW";
    return std::string(session.video().decoderStat());
}

void StreamDriver::focusChanged(bool focused)
{
    if (!focused || !connection)
        return;
    if (!play::StreamRecovery::watchesFocus(intentionalDisconnect, services.networkSettings.autoReconnect(),
            sessionStarted, host && host->isCloud()))
        return;
    const bool alive = play::StreamRecovery::sessionAlive(connection->state());
    if (!play::StreamRecovery::lost(videoActive, alive))
        return;
    log::info("[ARISA] the stream died while the app was in the background");
    connection->leave(false);
    stop();
    model.ended(ShellModel::Ending{ .title = services.strings.text("akira/stream/disconnected_reconnect"),
        .reconnect = true });
}

void StreamDriver::stop()
{
    closeDock();
    passcode.reset();
    prompt.reset();
    picker.reset();
    pausedForPasscode = false;
    if (couch.model)
        couch.model->reset();
    if (couch.table)
        couch.table->detach();
    videoOwnsScreen.reset();
    padsOwned.reset();
    if (videoActive)
        services.platform.display().streaming(false);
    videoActive = false;
    sessionStarted = false;
    services.streamState.setActive(false);
    connection.reset();
    play::Session& session = services.session.get();
    session.FreeController();
    controllerReady = false;
    session.FreeVideo();
    host = nullptr;
    menuHold = play::MenuHold();
    model.home();
    if (ended)
        ended();
    log::info("[ARISA] stream stopped; back on the console list");
}

void StreamDriver::fail(const std::string& text, bool offerReconnect)
{
    const std::size_t split = text.find("\n\n");
    model.ended(ShellModel::Ending{
        .title = split == std::string::npos ? text : text.substr(0, split),
        .message = split == std::string::npos ? std::string() : text.substr(split + 2),
        .reconnect = offerReconnect,
    });
}

void StreamDriver::onConnectProgress(const libchiaking::play::ConnectProgress& progress)
{
    log::info("[ARISA] connect progress: step {} attempt {}", static_cast<int>(progress.step), progress.attempt);
    flow.follow(progress.step, progress.reconnecting);
}

void StreamDriver::onLaunchProgress(const libchiaking::play::LaunchProgress&)
{
}

void StreamDriver::onPlaying()
{
    flow.connected();
    sessionStarted = true;
    log::info("[ARISA] session playing");
}

void StreamDriver::onEnded(const libchiaking::play::Outcome& outcome)
{
    if (!connection)
        return;
    const play::SessionEnding ending(outcome, play::SessionEnding::Situation{
        .loginPinEntered = false,
        .duringConnect = !videoActive,
        .consoleAwake = host && host->isAwake(),
        .cloudScreen = false,
        .cloudLaunch = false,
    });
    const std::string reason = services.strings.text(ending.reasonKey());
    stop();
    switch (ending.verdict())
    {
        case play::SessionEnding::Verdict::Stopped:
            notifier.notify(services.strings.text("akira/stream/quit_stopped"));
            return;
        case play::SessionEnding::Verdict::Unpaired:
            fail(services.strings.text("akira/stream/ps4_console_unregistered"), false);
            return;
        case play::SessionEnding::Verdict::Reconnect:
        case play::SessionEnding::Verdict::CloudFailure:
        case play::SessionEnding::Verdict::LaunchRefused:
        case play::SessionEnding::Verdict::Failed:
            break;
    }
    fail(ending.duringConnect()
            ? services.strings.format("akira/connection/connect_failed_title",
                  services.strings.text(ending.connectFailureKey()))
            : services.strings.format("akira/stream/session_ended", reason),
        false);
}

void StreamDriver::onRefused(const libchiaking::play::StartRefusal& refusal)
{
    if (!connection)
        return;
    stop();
    fail(services.strings.format("akira/connection/connect_failed_title",
             services.strings.text(play::ConnectFailure::forRefusal(refusal.kind()))),
        false);
}

void StreamDriver::onLoginPinNeeded(bool previousWasWrong)
{
    if (!connection)
        return;
    const std::string saved = host ? host->getConsolePIN() : std::string();
    if (play::StreamRecovery::loginPin(false, previousWasWrong, saved, true) == play::StreamRecovery::LoginPinStep::SubmitSaved
        && connection->submitLoginPin(saved))
    {
        notifier.notify(services.strings.text("akira/stream/auto_login_pin"));
        return;
    }
    log::warning("[ARISA] the console asked for a login PIN and none is saved");
    intentionalDisconnect = true;
    connection->leave(false);
    stop();
    fail(services.strings.text("akira/connection/login_pin_needed"), false);
}

void StreamDriver::onSeatPasscodeNeeded(uint8_t slot, bool retry)
{
    log::info("[COUCH] passcode request for player {} (retry: {})", slot + 1, retry);
    if (!connection)
        return;
    if (!couch.model)
    {
        connection->removePlayer(slot);
        return;
    }
    if (passcode)
    {
        log::info("[COUCH] passcode prompt already open, ignoring the repeat");
        return;
    }
    pausedForPasscode = !dock && !prompt;
    if (pausedForPasscode)
        pauseForUi();
    couch.model->askPasscode(slot, retry, [weak = weak_from_this()] {
        if (const auto self = weak.lock())
            self->passcodeClosed();
    });
    passcode.emplace(modal(composition::Element(play::ui::SeatPasscode(*couch.model, services.strings))));
}

void StreamDriver::onSeatRefused(uint8_t slot, const libchiaking::play::SeatRefusal& refusal)
{
    const std::uint8_t status = play::ConnectFailure::seatStatus(refusal);
    log::error("[COUCH] player {} could not join, status {}", slot + 1, status);
    notifier.notify(status == 1 ? services.strings.format("akira/couch/join_failed_not_signed_in", slot + 1)
                                : services.strings.format("akira/couch/join_failed_status", slot + 1, static_cast<int>(status)));
}

void StreamDriver::onSeatDropped(uint8_t slot)
{
    log::info("[COUCH] the console dropped player {}", slot + 1);
    notifier.notify(services.strings.format("akira/couch/join_failed_status", slot + 1,
        static_cast<int>(libchiaking::play::SeatRefusal::passcode_attempts_status)));
}

}
