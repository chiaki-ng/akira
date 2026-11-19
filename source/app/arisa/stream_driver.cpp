#include "app/arisa/stream_driver.hpp"

#include <arisa/composition/overlay_layer.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/overlay_behaviour.hpp>

#include <utility>

#include "app/arisa/shell_model.hpp"
#include "app/arisa/stream_panel.hpp"
#include "app/arisa/window.hpp"
#include "app/strings.hpp"
#include "consoles/host.hpp"
#include "log/log.hpp"
#include "network/wireguard_manager.hpp"
#include "platform/factory.hpp"
#include "play/connect_failure.hpp"
#include "play/input_manager.hpp"
#include "play/lazy_session.hpp"
#include "play/session.hpp"
#include "play/session_ending.hpp"
#include "play/stream_launcher.hpp"
#include "play/stream_route.hpp"
#include "play/stream_state.hpp"

namespace akira::app::arisa {

namespace composition = ::arisa::composition;
namespace provider = ::arisa::provider;
namespace runtime = ::arisa::runtime;

StreamDriver::StreamDriver(Services& services, runtime::Application& application, Window& window, ShellModel& model)
    : services(services)
    , application(application)
    , window(window)
    , model(model)
{
}

StreamDriver::~StreamDriver()
{
    if (connection)
        connection->leave(false);
    panel.reset();
    connection.reset();
}

void StreamDriver::start(consoles::Host& target)
{
    if (connection)
        return;

    play::Session& session = services.session.get();
    const play::StreamRoute route(std::nullopt, target.isRemote(), services.wireGuard.isConnected(), target.isPS5());
    services.streamState.setProfile(route.profile());

    log::info("[ARISA] starting stream to {}", target.getHostName());
    if (!session.InitController())
    {
        model.stopped(services.strings.text("akira/stream/failed_init_controller"));
        return;
    }

    play::StreamLauncher launcher(session, services.platform, services.preferences, services.accounts,
        services.dispatcher, services.core);
    auto opened = launcher.open(target, nullptr, weak_from_this(), libchiaking::play::ConsoleHandover::fresh,
        route.context());
    if (!opened)
    {
        log::error("[ARISA] the stream could not start: failure {}", static_cast<int>(opened.error()));
        session.FreeController();
        session.FreeVideo();
        model.stopped(services.strings.text(opened.error() == play::StreamLauncher::Failure::NotRegistered
                ? "akira/connection/fail_regist"
                : "akira/connection/fail_generic"));
        return;
    }

    connection = std::move(*opened);
    host = &target;
    controls = play::StreamControls();
    controls.offerSleep(true);
    flow.restart();
    padsOwned.emplace(application.block_input());
    model.connecting(target.getHostName());
}

void StreamDriver::cancel()
{
    if (!connection)
        return;
    connection->leave(false);
    stop({});
}

void StreamDriver::tick()
{
    if (!connection)
        return;

    if (window.takeVideoStart())
        enterVideo();

    if (!videoActive)
    {
        const play::ConnectFlow::Ring ring = flow.ring();
        model.stage(ring.labelKey ? services.strings.text(ring.labelKey) : ring.label);
        return;
    }

    if (!panel)
        followMenu();

    if (!services.session.get().MainLoop())
    {
        connection->leave(false);
        stop({});
    }
}

void StreamDriver::enterVideo()
{
    if (videoActive)
        return;
    videoActive = true;
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
        openPanel();
}

void StreamDriver::openPanel()
{
    play::Session& session = services.session.get();
    connection->setInputSuppressed(true);
    session.setVideoPaused(true);
    session.CleanUpHaptic();
    panelButtons.emplace(application.pass_input(provider::Buttons{ provider::Button::a, provider::Button::b,
        provider::Button::dpad_up, provider::Button::dpad_down }));
    panel.emplace(application.root().overlays().show(composition::OverlayLayer::dialog,
        StreamPanel(*this, services.strings, controls.offersSleep() && host && !host->cloudLaunch()),
        runtime::OverlayBehaviour{
            .input = composition::OverlayInput::modal,
            .alignment = composition::Alignment::center,
        }));
    log::info("[ARISA] stream panel opened");
}

void StreamDriver::resume()
{
    if (!connection)
        return;
    panel.reset();
    panelButtons.reset();
    services.session.get().setVideoPaused(false);
    connection->setInputSuppressed(false);
    log::info("[ARISA] stream resumed");
}

void StreamDriver::leave(bool rest)
{
    if (!connection)
        return;
    log::info("[ARISA] leaving the stream, rest={}", rest);
    connection->leave(rest);
    stop({});
}

void StreamDriver::stop(std::string status)
{
    panel.reset();
    panelButtons.reset();
    videoOwnsScreen.reset();
    padsOwned.reset();
    if (videoActive)
        services.platform.display().streaming(false);
    videoActive = false;
    services.streamState.setActive(false);
    connection.reset();
    play::Session& session = services.session.get();
    session.FreeController();
    session.FreeVideo();
    host = nullptr;
    menuHold = play::MenuHold();
    model.stopped(std::move(status));
    log::info("[ARISA] stream stopped; back on the console list");
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
    stop(services.strings.text(ending.reasonKey()));
}

void StreamDriver::onRefused(const libchiaking::play::StartRefusal& refusal)
{
    if (!connection)
        return;
    stop(services.strings.text(play::ConnectFailure::forRefusal(refusal.kind())));
}

void StreamDriver::onLoginPinNeeded(bool)
{
    if (!connection)
        return;
    log::warning("[ARISA] the console asked for its login PIN, which this shell cannot enter yet");
    connection->leave(false);
    stop(services.strings.text("akira/connection/fail_generic"));
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
