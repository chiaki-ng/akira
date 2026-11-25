#include "app/arisa/stream_driver.hpp"

#include <arisa/composition/overlay_layer.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/overlay_behaviour.hpp>

#include <algorithm>
#include <iterator>
#include <cstdint>
#include <format>
#include <utility>
#include <vector>

#include "accounts/account_settings.hpp"
#include "app/arisa/confirmation.hpp"
#include "app/arisa/couch_bridge.hpp"
#include "app/arisa/shell_model.hpp"
#include "app/arisa/window.hpp"
#include "app/app_settings.hpp"
#include "app/notifier.hpp"
#include "app/strings.hpp"
#include "consoles/console_actions.hpp"
#include "consoles/host.hpp"
#include "consoles/host_catalog.hpp"
#include "log/app_log.hpp"
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
#include "play/ui/login_pin_card.hpp"
#include "play/ui/pad_picker.hpp"
#include "play/ui/seat_passcode.hpp"
#include "play/ui/diagnostics_overlay.hpp"
#include "play/ui/stream_dock.hpp"
#include "play/stream_settings.hpp"
#include "settings/developer_settings.hpp"
#include "settings/settings_store.hpp"

namespace akira::app::arisa {

namespace composition = ::arisa::composition;
namespace provider = ::arisa::provider;
namespace runtime = ::arisa::runtime;

StreamDriver::StreamDriver(Services& services, StreamStage& stage, Window& window, ShellModel& model, Notifier& notifier,
    settings::ui::ControlOptions& controlOptions, settings::ui::PictureOptions& picture, settings::ui::SoundOptions& sound,
    play::ui::Mapping& mapping, Couch couch, std::function<void()> ended, CloudRetry retryCloud)
    : services(services)
    , stage(stage)
    , window(window)
    , model(model)
    , controlOptions(controlOptions)
    , picture(picture)
    , sound(sound)
    , mapping(mapping)
    , couch(couch)
    , notifier(notifier)
    , ended(std::move(ended))
    , retryCloud(std::move(retryCloud))
{
}

StreamDriver::~StreamDriver()
{
    focusWatch.reset();
    fileLogQuestion.reset();
    loginPin.reset();
    connectLogFollow.reset();
    confirmLeave.reset();
    hud.reset();
    if (videoActive)
        services.session.get().claimTouches(nullptr);
    if (connection)
        connection->leave(sessionStarted && services.appSettings.sleepOnExit() && host && !host->isCloud());
    passcode.reset();
    prompt.reset();
    closePicker();
    dock.reset();
    dockInput.reset();
    padsOwned.reset();
    stage.close();
    if (couch.table)
        couch.table->detach();
    connection.reset();
}

auto StreamDriver::modal(composition::Element content) -> runtime::OverlayHandle
{
    return stage.overlays().show(composition::OverlayLayer::dialog, std::move(content),
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

void StreamDriver::prepare(const consoles::Host& target, std::string activity)
{
    if (connection || waitingForPads || picker || fileLogQuestion)
        return;
    preparing = target.getHostName();
    model.connecting(target.getHostName());
    model.stage(play::ui::ConnectingScreen::Progress{
        .console = target.getHostName(), .status = activity, .step = 1, .activity = std::move(activity) });
}

bool StreamDriver::prepared(const consoles::Host& target) const
{
    return preparing == target.getHostName();
}

void StreamDriver::start(consoles::Host& target)
{
    preparing.reset();
    if (connection || waitingForPads || picker || fileLogQuestion)
        return;
    if (services.developer.fileLogging())
    {
        askAboutFileLogging(target);
        return;
    }
    begin(target);
}

void StreamDriver::askAboutFileLogging(consoles::Host& target)
{
    asking = &target;
    std::vector<Confirmation::Choice> choices;
    choices.push_back(Confirmation::Choice{ .label = services.strings.text("akira/connection/file_logging_keep_on"),
        .chosen = [weak = weak_from_this()] {
            if (const auto self = weak.lock())
                self->fileLoggingAnswered(false);
        },
        .key = "file-logging.keep" });
    choices.push_back(Confirmation::Choice{ .label = services.strings.text("akira/connection/file_logging_turn_off"),
        .chosen = [weak = weak_from_this()] {
            if (const auto self = weak.lock())
                self->fileLoggingAnswered(true);
        },
        .emphasis = Confirmation::Emphasis::Prominent,
        .key = "file-logging.turn-off" });
    fileLogQuestion.emplace(modal(Confirmation(services.strings.text("akira/connection/file_logging_title"),
        services.strings.text("akira/connection/file_logging_warning"), std::move(choices),
        [weak = weak_from_this()] {
            if (const auto self = weak.lock())
                self->fileLoggingAnswered(false);
        })
                                      .dimmed("file-logging")));
    log::info("[ARISA] file logging is on; asking before connecting to {}", target.getHostName());
}

void StreamDriver::fileLoggingAnswered(bool turnOff)
{
    fileLogQuestion.reset();
    consoles::Host* target = std::exchange(asking, nullptr);
    if (turnOff)
    {
        services.developer.setFileLogging(false);
        services.store.save();
        log::AppLog::close();
        log::info("[ARISA] file logging turned off before connecting");
    }
    if (target)
        begin(*target);
}

void StreamDriver::begin(consoles::Host& target)
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
    couch.table->watch(stage.captureButtons());
    pickerInput.emplace(stage.passInput(play::ui::PadPicker::passedButtons, play::ui::PadPicker::passedInput));
    picker.emplace(modal(composition::Element(play::ui::PadPicker(*couch.model, services.strings))));
}

void StreamDriver::closePicker()
{
    picker.reset();
    pickerInput.reset();
    if (couch.table)
        couch.table->unwatch();
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
    closePicker();
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
                    self->closePicker();
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
        padsOwned.emplace(stage.blockInput());
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
    session.setVolume(services.stream.volume());

    followConnectLog(target);
    log::info("[ARISA] starting stream to {}", target.getHostName());
    if (!prepareController())
    {
        stopConnectLog();
        fail(services.strings.text("akira/stream/failed_init_controller"), false);
        return;
    }

    play::StreamLauncher launcher(session, services.platform, services.preferences, services.accounts,
        services.dispatcher, services.core);
    auto opened = launcher.open(target, nullptr, weak_from_this(), handover, route.context());
    if (!opened)
    {
        log::error("[ARISA] the stream could not start: failure {}", static_cast<int>(opened.error()));
        stopConnectLog();
        session.FreeController();
        controllerReady = false;
        session.FreeVideo();
        fail(services.strings.format("akira/connection/connect_failed_title", services.strings.text(failureKey(opened.error()))),
            false);
        return;
    }

    connection = std::move(*opened);
    backgroundPause.emplace();
    host = &target;
    launchReport = cloud::LaunchReport();
    lastCloud = target.cloudLaunch() ? std::optional(*target.cloudLaunch()) : std::nullopt;
    lastConsole = target.getHostName();
    sessionStarted = false;
    intentionalDisconnect = false;
    loginPinEntered = false;
    controls = play::StreamControls();
    controls.offerSleep(!target.cloudLaunch());
    session.setStatsOverlayMode(overlayMode(services.stream.diagnostics().level));
    controls.showStats(session.getStatsOverlayMode());
    statsBoard.show(services.stream.diagnostics());
    history.reset();
    lastHistorySample.reset();
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

    followStage();
    if (!connection)
        return;

    if (!videoActive)
    {
        model.stage(launchReport.reported() ? launchProgress() : connectingProgress());
        return;
    }

    if (!dock && !prompt && !passcode && !picker)
        followMenu();
    sampleDiagnostics();
    followHud();
    if (couch.table && couch.table->degradedOnce())
        notifier.notify(services.strings.text("akira/settings/analog_triggers_degraded"));

    if (!services.session.get().MainLoop())
    {
        log::warning("[ARISA] the session main loop stopped");
        connection->leave(false);
        stop();
    }
}

void StreamDriver::followStage()
{
    stage.follow();
    if (window.takeVideoStart())
        videoStarting = true;
    if (const std::optional<std::string> refused = stage.takeRefusal())
    {
        log::error("[ARISA] the stream could not take over the display: {}", *refused);
        connection->leave(false);
        stop();
        fail(services.strings.format("akira/stream/session_ended", *refused), false);
        return;
    }
    if (videoStarting && stage.embedded())
    {
        videoStarting = false;
        enterVideo();
    }
}

void StreamDriver::enterVideo()
{
    if (videoActive)
        return;
    videoActive = true;
    stopConnectLog();
    padsOwned.emplace(stage.blockInput());
    services.platform.display().streaming(true);
    model.streaming();
    hudPlacement.restore(savedHudPosition());
    services.session.get().claimTouches(&hudPlacement);
    if (play::InputManager* input = services.session.get().getInputManager())
        input->setScreenshotChord(services.appSettings.screenshotChord());
    showHud();
    log::info("[ARISA] video owns the screen; akira presents and the overlay is embedded");
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
    if (!padsOwned)
        padsOwned.emplace(stage.blockInput());
    dockInput.emplace(stage.passInput(play::ui::StreamDock::passedButtons, play::ui::StreamDock::passedInput));
    hideHud();
    keepHudPosition();
    if (couch.model)
        couch.model->refresh();
    const bool players = couch.model && host && !host->isCloud();
    dock.emplace(stage.overlays().show(composition::OverlayLayer::dialog,
        play::ui::StreamDock(
            play::ui::StreamDock::Session{
                .console = host ? host->getHostName() : lastConsole,
                .controls = controls.controls(),
                .sleepOffered = controls.offersSleep(),
                .homeOffered = host && !host->isCloud(),
                .diagnostics = services.stream.diagnostics(),
                .players = players,
                .bandwidth = cloudBandwidth(),
                .opening = opening,
                .profile = host && host->isCloud() ? std::nullopt : std::optional(services.streamState.profile()),
            },
            *this, controlOptions, &picture, &sound, mapping, couch.model, services.strings),
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
    dockInput.reset();
    if (wasOpen && videoActive && connection)
        showHud();
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

auto StreamDriver::offersRest() const -> bool
{
    return sessionStarted && services.appSettings.sleepOnExit() && host && !host->isCloud() && controls.offersSleep();
}

void StreamDriver::leave(bool rest)
{
    if (!connection || confirmLeave)
        return;
    if (!rest && offersRest())
    {
        askRest();
        return;
    }
    finishLeave(rest);
}

void StreamDriver::askRest()
{
    const std::string console = host ? host->getHostName() : lastConsole;
    std::vector<Confirmation::Choice> choices;
    choices.push_back(Confirmation::Choice{ .label = services.strings.text("akira/stream/keep_on"),
        .chosen = [weak = weak_from_this()] {
            if (const auto self = weak.lock())
                self->finishLeave(false);
        },
        .key = "leave-confirm.keep" });
    choices.push_back(Confirmation::Choice{ .label = services.strings.text("akira/stream/turn_off"),
        .chosen = [weak = weak_from_this()] {
            if (const auto self = weak.lock())
                self->finishLeave(true);
        },
        .emphasis = Confirmation::Emphasis::Prominent,
        .key = "leave-confirm.turn-off" });
    confirmLeave.emplace(modal(Confirmation(services.strings.format("akira/stream/turn_off_title", console),
        services.strings.text("akira/stream/turn_off_message"), std::move(choices),
        [weak = weak_from_this()] {
            if (const auto self = weak.lock())
                self->confirmLeave.reset();
        })
                                   .dimmed("leave-confirm")));
    log::info("[ARISA] asking whether {} should rest", console);
}

void StreamDriver::finishLeave(bool rest)
{
    confirmLeave.reset();
    if (!connection)
        return;
    log::info("[ARISA] leaving the stream, rest={}", rest);
    const std::string console = host ? host->getHostName() : lastConsole;
    intentionalDisconnect = true;
    connection->leave(rest);
    stop();
    notifier.notify(services.strings.format(rest ? "akira/stream/going_to_rest" : "akira/stream/disconnected_from", console));
}

void StreamDriver::cancel()
{
    if (!connection && preparing)
    {
        log::info("[ARISA] title start cancelled before connecting");
        preparing.reset();
        model.home();
        notifier.notify(services.strings.text("akira/connection/cancelled"));
        return;
    }
    if (!connection)
        return;
    log::info("[ARISA] connection cancelled");
    intentionalDisconnect = true;
    connection->leave(false);
    stop();
    notifier.notify(services.strings.text("akira/connection/cancelled"));
}

void StreamDriver::reconnect()
{
    if (connection)
        return;
    if (lastCloud)
    {
        if (!retryCloud)
            return;
        log::info("[ARISA] retrying the cloud launch of {}", lastCloud->name());
        notifier.notify(services.strings.text("akira/stream/reconnecting"));
        retryCloud(*lastCloud);
        return;
    }
    const auto& entries = services.hosts.entries();
    const auto found = entries.find(lastConsole);
    if (found == entries.end() || !found->second)
        return;
    notifier.notify(services.strings.text("akira/stream/reconnecting"));
    open(*found->second, libchiaking::play::ConsoleHandover::just_left);
}

void StreamDriver::restart()
{
    if (!connection || !host || host->isCloud())
        return;
    log::info("[ARISA] restarting the stream with new settings");
    intentionalDisconnect = true;
    connection->leave(false);
    stop();
    reconnect();
}

void StreamDriver::goHome()
{
    if (!connection)
        return;
    if (!services.session.get().goHome())
    {
        notifier.notify(services.strings.text("akira/stream_menu/go_home_refused"));
        return;
    }
    log::info("[ARISA] asked the console to go to its Home screen");
}

void StreamDriver::tuneColour(play::ColourAdjustments colour)
{
    services.session.get().setColour(colour);
}

void StreamDriver::tuneVolume(play::Volume volume)
{
    services.session.get().setVolume(volume);
}

void StreamDriver::resetMotion()
{
    if (play::InputManager* input = services.session.get().getInputManager())
        input->resetMotionControls();
}

void StreamDriver::showDiagnostics(play::DiagnosticsAppearance appearance)
{
    const play::StatsOverlayMode mode = overlayMode(appearance.level);
    services.session.get().setStatsOverlayMode(mode);
    controls.showStats(mode);
    statsBoard.show(appearance);
    if (services.stream.diagnostics() == appearance)
        return;
    services.stream.setDiagnostics(appearance);
    services.store.save();
}

auto StreamDriver::cloudBandwidth() -> std::optional<std::string>
{
    if (!host || !host->isCloud())
        return std::nullopt;
    const play::StreamStats stats = services.session.get().getStreamStats();
    if (!stats.console_quality_valid || stats.target_bitrate_kbps == 0)
        return std::nullopt;
    return std::format("{:.1f} Mbps", stats.target_bitrate_kbps / 1000.0);
}

auto StreamDriver::overlayMode(play::DiagnosticsAppearance::Level level) -> play::StatsOverlayMode
{
    switch (level)
    {
        case play::DiagnosticsAppearance::Level::Off: return play::StatsOverlayMode::Off;
        case play::DiagnosticsAppearance::Level::Compact: return play::StatsOverlayMode::Compact;
        case play::DiagnosticsAppearance::Level::Standard:
        case play::DiagnosticsAppearance::Level::Full: break;
    }
    return play::StatsOverlayMode::Full;
}

auto StreamDriver::savedHudPosition() const -> std::optional<play::TouchClaim::Fraction>
{
    const float x = services.stream.statsOverlayX();
    const float y = services.stream.statsOverlayY();
    if (x < 0.0f || y < 0.0f)
        return std::nullopt;
    return play::TouchClaim::Fraction{ .x = x, .y = y };
}

auto StreamDriver::diagnostics() -> play::DiagnosticsReport
{
    play::Session& session = services.session.get();
    return play::DiagnosticsReport(play::DiagnosticsReport::Sample{
                                       .stats = session.getStreamStats(),
                                       .upscaling = session.upscaling(),
                                       .decoder = decoder(),
                                       .context = session.connectionContext(),
                                   },
        history);
}

void StreamDriver::sampleDiagnostics()
{
    const Clock::time_point now = Clock::now();
    if (lastHistorySample && now - *lastHistorySample < historyInterval)
        return;
    const double seconds = lastHistorySample
        ? std::chrono::duration<double>(now - *lastHistorySample).count()
        : std::chrono::duration<double>(historyInterval).count();
    lastHistorySample = now;
    history.record(services.session.get().getStreamStats(), seconds);
}

void StreamDriver::showHud()
{
    const play::DiagnosticsAppearance appearance = services.stream.diagnostics();
    statsBoard.show(appearance);
    if (appearance.level == play::DiagnosticsAppearance::Level::Off || hud || dock || !videoActive)
        return;
    statsBoard.refresh(diagnostics());
    statsBoard.place(hudPlacement.origin(), hudPlacement.scale());
    nextStatsSample = Clock::now() + statsInterval;
    hud.emplace(stage.overlays().show(composition::OverlayLayer::hud,
        play::ui::DiagnosticsOverlay(statsBoard, hudPlacement, services.strings),
        runtime::OverlayBehaviour{
            .input = composition::OverlayInput::passthrough,
            .alignment = composition::Alignment::top_leading,
            .transition = ::arisa::motion::Transition::opacity(),
        }));
    hudPlacement.show(true);
    log::info("[ARISA] stats HUD shown");
}

void StreamDriver::hideHud()
{
    if (!hud)
        return;
    hud.reset();
    hudPlacement.show(false);
}

void StreamDriver::followHud()
{
    if (!hud)
        return;
    const Clock::time_point now = Clock::now();
    if (now >= nextStatsSample)
    {
        statsBoard.refresh(diagnostics());
        nextStatsSample = now + statsInterval;
    }
    if (const std::uint64_t revision = hudPlacement.revision(); revision != hudRevision)
    {
        hudRevision = revision;
        statsBoard.place(hudPlacement.origin(), hudPlacement.scale());
    }
    if (const std::optional<play::StatsHudPlacement::Tap> tap = hudPlacement.takeTapped())
        showDiagnostics(tapped(services.stream.diagnostics(), *tap));
    adoptHudPosition();
}

auto StreamDriver::tapped(play::DiagnosticsAppearance appearance, play::StatsHudPlacement::Tap tap)
    -> play::DiagnosticsAppearance
{
    using Level = play::DiagnosticsAppearance::Level;
    using Layout = play::DiagnosticsAppearance::Layout;
    switch (tap)
    {
        case play::StatsHudPlacement::Tap::Compact: appearance.level = Level::Compact; break;
        case play::StatsHudPlacement::Tap::Standard: appearance.level = Level::Standard; break;
        case play::StatsHudPlacement::Tap::Full: appearance.level = Level::Full; break;
        case play::StatsHudPlacement::Tap::Layout:
            if (appearance.level != Level::Compact)
                appearance.layout = appearance.layout == Layout::Sheet ? Layout::Floating : Layout::Sheet;
            break;
        case play::StatsHudPlacement::Tap::Glass:
        {
            const auto& glasses = play::DiagnosticsAppearance::glasses;
            const auto at = std::ranges::find(glasses, appearance.glass);
            appearance.glass = at == glasses.end() || std::next(at) == glasses.end() ? glasses.front() : *std::next(at);
            break;
        }
    }
    return appearance;
}

void StreamDriver::adoptHudPosition()
{
    if (const std::optional<play::TouchClaim::Fraction> settled = hudPlacement.takeSettled())
    {
        services.stream.setStatsOverlayPosition(settled->x, settled->y);
        hudPositionUnsaved = true;
    }
}

void StreamDriver::keepHudPosition()
{
    adoptHudPosition();
    if (!hudPositionUnsaved)
        return;
    hudPositionUnsaved = false;
    services.store.save();
    log::info("[ARISA] stats HUD position saved");
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
    confirmLeave.reset();
    loginPin.reset();
    stopConnectLog();
    closeDock();
    passcode.reset();
    prompt.reset();
    closePicker();
    pausedForPasscode = false;
    if (couch.model)
        couch.model->reset();
    if (couch.table)
        couch.table->detach();
    hideHud();
    keepHudPosition();
    if (videoActive)
        services.session.get().claimTouches(nullptr);
    padsOwned.reset();
    stage.close();
    videoStarting = false;
    if (videoActive)
        services.platform.display().streaming(false);
    videoActive = false;
    sessionStarted = false;
    services.streamState.setActive(false);
    connection.reset();
    backgroundPause.reset();
    play::Session& session = services.session.get();
    session.FreeController();
    controllerReady = false;
    session.FreeVideo();
    host = nullptr;
    owned.reset();
    menuHold = play::MenuHold();
    model.home();
    if (ended)
        ended();
    log::info("[ARISA] stream stopped; back on the console list");
}

void StreamDriver::fail(const std::string& text, bool offerReconnect, bool retry)
{
    const std::size_t split = text.find("\n\n");
    model.ended(ShellModel::Ending{
        .title = split == std::string::npos ? text : text.substr(0, split),
        .message = split == std::string::npos ? std::string() : text.substr(split + 2),
        .reconnect = offerReconnect,
        .retry = retry,
    });
}

auto StreamDriver::cloudStopped(const play::SessionEnding& ending, const libchiaking::play::Outcome& outcome) const
    -> std::string
{
    std::string message = ending.duringConnect()
        ? services.strings.text(ending.failureKey())
        : services.strings.format("akira/stream/session_ended", services.strings.text(ending.reasonKey()));
    if (outcome.launch_refused() && host && host->cloudLaunch())
        message = cloud::CloudLaunch::refusalText(services.strings, *outcome.launch_refused(), host->cloudLaunch()->name());
    return services.strings.format("akira/cloud/start_stopped", message);
}

void StreamDriver::onConnectProgress(const libchiaking::play::ConnectProgress& progress)
{
    log::info("[ARISA] connect progress: step {} attempt {}", static_cast<int>(progress.step), progress.attempt);
    flow.follow(progress.step, progress.reconnecting);
}

void StreamDriver::startCloud(std::shared_ptr<consoles::Host> target)
{
    if (!target || connection || waitingForPads || picker || fileLogQuestion)
        return;
    owned = std::move(target);
    start(*owned);
    if (!connection && !waitingForPads && !fileLogQuestion)
        owned.reset();
}

auto StreamDriver::launchProgress() const -> play::ui::ConnectingScreen::Progress
{
    return play::ui::ConnectingScreen::Progress{
        .console = host ? host->getHostName() : lastConsole,
        .status = launchReport.status(services.strings),
        .step = launchReport.step(),
        .steps = cloud::LaunchReport::steps,
        .activity = launchReport.activity(services.strings),
        .details = launchReport.details(services.strings, Clock::now()),
    };
}

void StreamDriver::onLaunchProgress(const libchiaking::play::LaunchProgress& progress)
{
    if (!connection)
        return;
    launchReport.follow(progress, Clock::now());
    model.stage(launchProgress());
}

auto StreamDriver::failureKey(play::StreamLauncher::Failure failure) -> const char*
{
    switch (failure)
    {
        case play::StreamLauncher::Failure::NotRegistered: return "akira/connection/fail_regist";
        case play::StreamLauncher::Failure::Worker: return "akira/connection/failed_create_thread";
        case play::StreamLauncher::Failure::NoPreferences:
        case play::StreamLauncher::Failure::Codec:
        case play::StreamLauncher::Failure::Video: break;
    }
    return "akira/connection/fail_generic";
}

void StreamDriver::onPlaying()
{
    flow.connected();
    sessionStarted = true;
    log::info("[ARISA] session playing");
    notifier.notify(services.strings.format("akira/stream/connected_to", host ? host->getHostName() : lastConsole));
}

void StreamDriver::onEnded(const libchiaking::play::Outcome& outcome)
{
    if (!connection)
        return;
    const bool cloud = host && host->isCloud();
    const play::SessionEnding ending(outcome, play::SessionEnding::Situation{
        .loginPinEntered = loginPinEntered,
        .duringConnect = !videoActive,
        .consoleAwake = host && host->isAwake(),
        .cloudScreen = cloud,
        .cloudLaunch = host && host->cloudLaunch(),
    });
    const std::string reason = services.strings.text(ending.reasonKey());
    const std::string stopped = cloud ? cloudStopped(ending, outcome) : std::string();
    if (ending.verdict() == play::SessionEnding::Verdict::Reconnect)
    {
        log::info("[ARISA] the first login PIN was entered; reconnecting once");
        loginPinEntered = false;
        stop();
        reconnect();
        return;
    }
    stop();
    switch (ending.verdict())
    {
        case play::SessionEnding::Verdict::Stopped:
            notifier.notify(services.strings.text("akira/stream/quit_stopped"));
            return;
        case play::SessionEnding::Verdict::Unpaired:
            fail(services.strings.text("akira/stream/ps4_console_unregistered"), false);
            return;
        case play::SessionEnding::Verdict::CloudFailure:
            log::info("[ARISA] the cloud session ended, retry offered: {}", !ending.endsPlayTime());
            fail(stopped, !ending.endsPlayTime(), true);
            return;
        case play::SessionEnding::Verdict::Reconnect:
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
    if (host && host->isCloud())
    {
        const cloud::CloudLaunch* launch = host->cloudLaunch();
        const std::string text = refusal.launch() && launch
            ? cloud::CloudLaunch::refusalText(services.strings, *refusal.launch(), launch->name())
            : services.strings.text(play::ConnectFailure::forRefusal(refusal.kind()));
        stop();
        fail(services.strings.format("akira/cloud/start_stopped", text), true, true);
        return;
    }
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
    switch (play::StreamRecovery::loginPin(host && host->isCloud(), previousWasWrong, saved, true))
    {
        case play::StreamRecovery::LoginPinStep::Ignore:
            log::warning("[ARISA] ignoring a login PIN request for a cloud session");
            return;
        case play::StreamRecovery::LoginPinStep::SubmitSaved:
            if (connection->submitLoginPin(saved))
            {
                notifier.notify(services.strings.text("akira/stream/auto_login_pin"));
                return;
            }
            log::warning("[ARISA] the saved login PIN could not be sent; asking instead");
            break;
        case play::StreamRecovery::LoginPinStep::Ask:
            break;
    }
    askLoginPin(previousWasWrong);
}

void StreamDriver::askLoginPin(bool retry)
{
    loginPin.reset();
    loginPin.emplace(modal(composition::Element(play::ui::LoginPinCard(retry, *this, services.strings))));
    log::info("[ARISA] asking for the login PIN, retry={}", retry);
}

void StreamDriver::submitLoginPin(consoles::ConsolePin pin)
{
    loginPin.reset();
    if (!connection)
        return;
    if (!connection->submitLoginPin(pin.text()))
        log::warning("[ARISA] the login PIN could not be sent");
    loginPinEntered = true;
    if (host && !consoles::ConsoleActions(services.hosts, services.store, services.accounts, services.developer,
                     services.wireGuard, services.core)
                     .setConsolePin(*host, pin.text()))
        log::warning("[ARISA] the login PIN for {} was not saved", host->getHostName());
}

void StreamDriver::cancelLoginPin()
{
    loginPin.reset();
    log::info("[ARISA] login PIN cancelled");
    cancel();
}

void StreamDriver::followConnectLog(const consoles::Host& target)
{
    stopConnectLog();
    if (services.appSettings.connectionShowStages() || target.isCloud())
        return;
    connectLog.emplace(connectLogLines, log::LogTail::Stamp::Clock);
    if (services.appSettings.hideAccountName())
    {
        for (const accounts::Profile& profile : services.accounts.profiles())
        {
            connectLog->mask(profile.onlineId, services.appSettings.maskAccountName(profile.onlineId));
            connectLog->mask(profile.accountId, services.appSettings.maskAccountName(profile.accountId));
        }
    }
    connectLogFollow.emplace(log::Log::process().subscribe([tail = &*connectLog](const log::Entry& entry) {
        tail->push(entry.message);
    }));
    connectLog->push(services.strings.text("akira/connection/starting"));
}

void StreamDriver::stopConnectLog()
{
    connectLogFollow.reset();
    connectLog.reset();
}

auto StreamDriver::connectingProgress() const -> play::ui::ConnectingScreen::Progress
{
    play::ui::ConnectingScreen::Progress shown =
        play::ui::ConnectingScreen::progressOf(flow.ring(), host ? host->getHostName() : lastConsole, services.strings);
    if (connectLog)
    {
        shown.stages = false;
        shown.log = connectLog->lines();
    }
    return shown;
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
