#include "app/arisa/stream_driver.hpp"

#include <arisa/composition/overlay_layer.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/overlay_behaviour.hpp>

#include <utility>

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
#include "play/ui/connecting_screen.hpp"
#include "play/ui/stream_dock.hpp"

namespace akira::app::arisa {

namespace composition = ::arisa::composition;
namespace provider = ::arisa::provider;
namespace runtime = ::arisa::runtime;

StreamDriver::StreamDriver(Services& services, runtime::Application& application, Window& window, ShellModel& model,
    Notifier& notifier, std::function<void()> ended)
    : services(services)
    , application(application)
    , window(window)
    , model(model)
    , notifier(notifier)
    , ended(std::move(ended))
{
}

StreamDriver::~StreamDriver()
{
    focusWatch.reset();
    if (connection)
        connection->leave(false);
    dock.reset();
    connection.reset();
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
    open(target, libchiaking::play::ConsoleHandover::fresh);
}

void StreamDriver::open(consoles::Host& target, libchiaking::play::ConsoleHandover handover)
{
    if (connection)
        return;

    play::Session& session = services.session.get();
    const play::StreamRoute route(std::nullopt, target.isRemote(), services.wireGuard.isConnected(), target.isPS5());
    services.streamState.setProfile(route.profile());

    log::info("[ARISA] starting stream to {}", target.getHostName());
    if (!session.InitController())
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
    flow.restart();
    model.connecting(target.getHostName());
}

void StreamDriver::tick()
{
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

    if (!dock)
        followMenu();

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

void StreamDriver::openDock()
{
    play::Session& session = services.session.get();
    connection->setInputSuppressed(true);
    session.setVideoPaused(true);
    session.CleanUpHaptic();
    dockButtons.emplace(application.pass_input(provider::Buttons{ provider::Button::a, provider::Button::b,
        provider::Button::dpad_up, provider::Button::dpad_down, provider::Button::dpad_left,
        provider::Button::dpad_right }));
    dock.emplace(application.root().overlays().show(composition::OverlayLayer::dialog,
        play::ui::StreamDock(
            play::ui::StreamDock::Session{
                .console = host ? host->getHostName() : lastConsole,
                .controls = controls.controls(),
                .sleepOffered = controls.offersSleep(),
                .statsMode = session.getStatsOverlayMode(),
            },
            *this, services.strings),
        runtime::OverlayBehaviour{
            .input = composition::OverlayInput::modal,
            .alignment = composition::Alignment::bottom,
        }));
    log::info("[ARISA] stream dock opened");
}

void StreamDriver::closeDock()
{
    dock.reset();
    dockButtons.reset();
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

void StreamDriver::onSeatPasscodeNeeded(uint8_t, bool)
{
}

void StreamDriver::onSeatRefused(uint8_t, const libchiaking::play::SeatRefusal&)
{
}

void StreamDriver::onSeatDropped(uint8_t)
{
}

}
