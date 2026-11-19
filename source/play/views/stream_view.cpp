#include "play/views/stream_view.hpp"
#include "play/lazy_session.hpp"
#include "app/view_services.hpp"
#include "play/stream_preferences.hpp"
#include "play/views/stream_nav.hpp"
#include "platform/pads/pad.hpp"
#include "platform/factory.hpp"
#include "play/views/stream_menu.hpp"
#include "play/views/connecting_view.hpp"
#include "log/log.hpp"
#if AKIRA_CLOUD
#include "cloud/cloud_connect_task.hpp"
#endif
#include "app/views/theme.hpp"
#include "play/views/connection_stage.hpp"
#include "play/connect_failure.hpp"
#include "play/session_ending.hpp"
#include "play/stream_recovery.hpp"
#include "play/stream_route.hpp"
#include "platform/video/renderer.hpp"
#include "consoles/views/enter_pin_view.hpp"
#include "play/views/controller_remap_view.hpp"
#include <format>
#include "app/exception.hpp"
#include "network/wireguard_manager.hpp"
#include "play/input_manager.hpp"
#include "play/chiaki_core.hpp"
#include "app/views/shared_view_holder.hpp"
#include <switch.h>
#include <algorithm>
#include <chrono>
#include <borealis/core/i18n.hpp>

namespace akira::play::views {

using namespace brls::literals;

StreamView::StreamView(consoles::Host* host, std::shared_ptr<consoles::Host> hostOwner)
    : host(host)
    , hostOwner(std::move(hostOwner))
{
    this->session = &app::ViewServices::current().session.get();
    this->services = &app::ViewServices::current();
#if AKIRA_CLOUD
    if (const auto* launch = host->cloudLaunch())
        cloudScreen.emplace(launch->name());
#endif

    setFocusable(true);
}

void StreamView::setupCallbacks()
{
    auto weak = weak_from_this();

    session->getInputManager()->setTargetPS5(host->isPS5());

    if (auto& services = app::ViewServices::current();
        services.platform.supplies(akira::platform::PlatformCapability::couch_seats)) {
        controls = std::make_unique<CouchSeatControls>(*services.platform.couchSeats(),
            PlayersPanelView::Profiles{ .accounts = services.accounts, .app = services.appSettings,
                .store = services.store, .trophies = services.trophies },
            weak_from_this());
        controls->attach();
    }

    exitSubscription = brls::Application::getExitEvent()->subscribe([weak]() {
        if (auto self = weak.lock()) {
            if (self->sessionStarted && self->services->appSettings.sleepOnExit() && !self->host->isCloud() && self->connection) {
                self->connection->leave(true);
            }
        }
    });

    focusSubscription = brls::Application::getWindowFocusChangedEvent()->subscribe([weak](bool focused) {
        if (auto self = weak.lock()) {
            self->onFocusChanged(focused);
        }
    });

    startLogCapture();
}

void StreamView::startLogCapture()
{
    if (logSubscription)
        return;

    auto weak = weak_from_this();
    logSubscription = log::Log::process().subscribe(
        [weak](const log::Entry& entry) {
            if (auto self = weak.lock())
                self->logTail.push(entry.message);
        });
}

void StreamView::stopLogCapture()
{
    logSubscription.reset();
}

void StreamView::adoptConnectionLog(log::ConnectionLog log)
{
    connectionLog = std::move(log);
}

StreamView::~StreamView()
{
    brls::Application::getExitEvent()->unsubscribe(exitSubscription);
    brls::Application::getWindowFocusChangedEvent()->unsubscribe(focusSubscription);
    stopLogCapture();

    stopStream();
    if (controllerReady)
        session->FreeController();

    // Release shared_ptr held by SharedViewHolder
    app::views::SharedViewHolder::release(this);
}

brls::View* StreamView::create()
{
    // This requires a host to be passed, so we return nullptr for XML creation
    return nullptr;
}

void StreamView::startStream()
{
    if (sessionStarted)
    {
        return;
    }

    // The connection UI needs recent messages and stage matching. Once video
    // starts, activateVideoPipeline() detaches this subscriber so packet-level
    // diagnostics still reach the file without being copied and parsed here.
    startLogCapture();

    if (controls && controls->beforeStart())
    {
        return;
    }

    if (sessionPreStarted)
    {
        log::info("Session already started while connecting, activating stream...");
        if (!controllerReady)
            controllerReady = session->InitController();
        sessionStarted = true;
        streamActive = true;
        prepareVideoPipelineTick();
        brls::Application::blockInputs(true);
        log::info("Stream activated successfully");
        return;
    }

    log::info("Starting stream to {}", host->getHostName());

    try
    {
        if (host->isCloud())
            services->streamState.setCloudPscloud(host->cloudLaunch()->ps5());
        services->streamState.setProfile(route().profile());

        if (!controllerReady)
        {
            if (!session->InitController())
            {
                log::error("Failed to initialize controller");
                throw app::Exception("akira/stream/failed_init_controller"_i18n);
            }
            controllerReady = true;
        }

        openConnection();

        sessionStarted = true;
        streamActive = true;
        prepareVideoPipelineTick();

        brls::Application::blockInputs(true);

        log::info("Stream started successfully");
    }
    catch (const app::Exception& e)
    {
        log::error("Failed to start stream: {}", e.what());

        connection.reset();
        session->FreeController();
        session->FreeVideo();

        std::string errorMsg = e.what();
        brls::sync([errorMsg]() {
            auto* dialog = new brls::Dialog(brls::getStr("akira/stream/connection_failed", errorMsg));
            dialog->addButton("OK", []() {
                stream_nav::unwindToBase();
            });
            dialog->open();
            brls::Application::forceUnblockInputs();
        });
    }
}

RumbleProfile StreamView::activePadProfile()
{
    return app::ViewServices::current().platform.pads().primaryRumbleProfile();
}

void StreamView::openConnection()
{
    namespace play = libchiaking::play;

    auto* accounts = ChiakiCore::current().accounts();
    auto player = accounts ? accounts->activeUser() : nullptr;
    const auto& console = host->getConsole();
    Connection::Starter starter;
    int width = 0;
    int height = 0;
    int fps = 60;
    int bitrate = 0;
    bool hevc = false;
    if (const auto* launch = host->cloudLaunch()) {
        auto preferences = app::ViewServices::current().preferences.forCloud(*launch);
        if (!preferences)
            throw app::Exception("akira/connection/fail_generic"_i18n);
        const auto picture = preferences->picture.on_cloud(launch->service());
        width = static_cast<int>(picture.width());
        height = static_cast<int>(picture.height());
        fps = static_cast<int>(picture.fps());
        bitrate = static_cast<int>(picture.bitrate_kbps());
        hevc = picture.codec() != libchiaking::media::VideoCodec::h264;
        starter = [owner = hostOwner, launch, preferences = *preferences](
                      libchiaking::media::MediaSink& sink, play::SessionEvents& events,
                      const libchiaking::platform::StopToken&) -> Connection::Started {
            return launch->start(preferences, sink, events);
        };
    } else {
        if (!player || !console)
            throw app::Exception("akira/connection/fail_regist"_i18n);
        auto preferences = app::ViewServices::current().preferences.forConsole(*host);
        if (!preferences)
            throw app::Exception("akira/connection/fail_generic"_i18n);
        preferences->handover = handover;
        const auto picture = preferences->picture.on_console(console->generation());
        width = static_cast<int>(picture.width());
        height = static_cast<int>(picture.height());
        fps = static_cast<int>(picture.fps());
        bitrate = static_cast<int>(picture.bitrate_kbps());
        hevc = picture.codec() != libchiaking::media::VideoCodec::h264;
        starter = Connection::consoleStarter(console, player, *preferences,
            host->isRemote() ? libchiaking::consoles::Route::internet : libchiaking::consoles::Route::home);
    }
    handover = play::ConsoleHandover::fresh;

    log::info("StreamView: requesting {}x{} at {} fps, {} kbps, {}", width, height, fps, bitrate,
                       hevc ? "HEVC" : "H.264");
    session->setRequestedProfile(width, height, fps, bitrate, hevc);

    const RumbleProfile padProfile = activePadProfile();
    session->setRumbleStrength(padProfile.rumble_source == RumbleSource::Off
                                   ? 0.0f
                                   : padProfile.strength);
    session->setRumbleSource(padProfile.rumble_source);
    session->setHapticIntensity(padProfile.haptic_intensity);
    session->setHapticBase(HapticBaseForIntensity(padProfile.haptic_intensity));
    session->setRumbleFreqs(padProfile.freq_low, padProfile.freq_high);
    session->setEnvelopeAttack(padProfile.envelope_attack);
    session->setEnvelopeDecay(padProfile.envelope_decay);
    session->setRumbleCeiling(padProfile.ceiling);

    if (!session->InitAVCodec(hevc, host->isPS5(), width, height))
        throw app::Exception("Failed to initiate libav codec");
    if (!session->InitVideo(width, height))
        throw app::Exception("Failed to initiate video");

    session->setConnectionContext(route().context());

    std::shared_ptr<ConnectionListener> self = shared_from_this();
    connection = std::make_shared<Connection>(*session, self, host->isPS5(),
        app::ViewServices::current().platform, services->accounts, services->dispatcher);
    if (!connection->open(std::move(starter))) {
        connection.reset();
        throw app::Exception("akira/connection/failed_create_thread"_i18n);
    }
}

void StreamView::stopStream()
{
    if (!sessionStarted)
    {
        return;
    }

    log::info("Stopping stream");

    brls::Application::forceUnblockInputs();
    brls::Application::setRenderSuspended(false);
    if (session) {
        brls::Application::setSuspendedRenderCallback(nullptr);
    }
    brls::Application::setLimitedFPS(0);
    brls::Application::setSwapInterval(1);
    app::ViewServices::current().platform.display().streaming(false);
    brls::Application::setExclusiveRender(false);
    videoPipelineActive = false;

    streamActive = false;

    services->streamState.setActive(false);
    connection.reset();

    session->FreeController();
    session->FreeVideo();

    controllerReady = false;

    sessionStarted = false;
}

void StreamView::capturePausedFrame()
{
    pausedFrame = PausedFrame{};

    if (!session)
        return;

    akira::platform::VideoRenderer* renderer = session->getVideoRenderer();
    if (!renderer)
        return;

    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    if (!renderer->captureLastFrame(pixels, width, height))
        return;

    pausedFrame = PausedFrame(std::move(pixels), width, height);
}

void StreamView::releasePausedFrame(NVGcontext* vg)
{
    if (pausedFrameImage >= 0 && vg)
        nvgDeleteImage(vg, pausedFrameImage);
    pausedFrameImage = -1;
    pausedFrame = PausedFrame{};
}

bool StreamView::drawPausedFrame(NVGcontext* vg, float x, float y, float width, float height)
{
    if (pausedFrameImage < 0 && pausedFrame.ready())
    {
        pausedFrameImage = nvgCreateImageRGBA(vg, pausedFrame.width(), pausedFrame.height(), 0, pausedFrame.data());
        if (pausedFrameImage < 0)
            log::warning("StreamView: failed to create paused frame image");
        pausedFrame.dropPixels();
    }

    if (pausedFrameImage < 0)
        return false;

    NVGpaint paint = nvgImagePattern(vg, x, y, width, height, 0.0f, pausedFrameImage, 1.0f);
    nvgBeginPath(vg);
    nvgRect(vg, x, y, width, height);
    nvgFillPaint(vg, paint);
    nvgFill(vg);
    return true;
}

void StreamView::draw(NVGcontext* vg, float x, float y, float width, float height,
                      brls::Style style, brls::FrameContext* ctx)
{
    ++drawCount;
    if (services->developer.renderLog() && drawCount % 60 == 0) {
        log::info("StreamView::draw #{}: streamActive={}, sessionStarted={}, menuOpen={}",
            drawCount, streamActive, sessionStarted, menuOpen);
    }

#if AKIRA_CLOUD
    if (cloudScreen && (!streamActive || !sessionStarted || !session->hasReceivedFirstFrame()))
    {
        drawCloudLaunch(vg, x, y, width, height);
        return;
    }
#endif

    if (!streamActive || !sessionStarted)
    {
        nvgBeginPath(vg);
        nvgRect(vg, x, y, width, height);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
        nvgFill(vg);

        if (services->appSettings.connectionShowStages()) {
            drawStageRing(vg, x, y, width, height);
        } else {
            nvgFontSize(vg, 24);
            nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
            std::string connectingText = "akira/stream/connecting"_i18n;
            nvgText(vg, x + width / 2, y + 40, connectingText.c_str(), nullptr);
            renderLogs(vg, x, y + 90, width, height - 90);
        }

        return;
    }

    if (!menuOpen) {
        checkMenuTrigger();
    }

    if (menuOpen)
    {
        wasMenuOpen = true;
        if (!drawPausedFrame(vg, x, y, width, height))
        {
            nvgBeginPath(vg);
            nvgRect(vg, x, y, width, height);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
            nvgFill(vg);
        }
        return;
    }

    if (wasMenuOpen)
    {
        log::info("StreamView::draw: resuming after menu close, stats_overlay={}",
            (int)session->getStatsOverlayMode());
        wasMenuOpen = false;
        releasePausedFrame(vg);

        nvgBeginPath(vg);
        nvgRect(vg, x, y, width, height);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
        nvgFill(vg);
        log::info("StreamView::draw: skipping first frame after menu");
        return;
    }

    if (!session->hasReceivedFirstFrame())
    {
        nvgBeginPath(vg);
        nvgRect(vg, x, y, width, height);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
        nvgFill(vg);

        if (services->appSettings.connectionShowStages()) {
            drawStageRing(vg, x, y, width, height);
        } else {
            renderLogs(vg, x, y, width, height);
        }

        session->MainLoop();
        return;
    }

    activateVideoPipeline();
}

StreamRoute StreamView::route() const
{
    const auto* launch = host->cloudLaunch();
    return StreamRoute(launch ? std::optional<std::string>(launch->name()) : std::nullopt, host->isRemote(),
                       app::ViewServices::current().wireGuard.isConnected(), host->isPS5());
}

void StreamView::drawStageRing(NVGcontext* vg, float x, float y, float width, float height)
{
    const ConnectFlow::Ring ring = flow.ring();
    drawConnectionRing(vg, x + width / 2, y + height / 2, brls::getStr(ring.labelKey), ring.index, ring.total);
}

void StreamView::prepareVideoPipelineTick()
{
    auto weak = weak_from_this();
    brls::Application::setSuspendedRenderCallback([weak]() {
        if (auto self = weak.lock())
            self->streamingTick();
    });
    brls::Application::setLimitedFPS(60);
    brls::Application::setSwapInterval(0);
    app::ViewServices::current().platform.display().streaming(true);
    brls::Application::setExclusiveRender(true);
}

void StreamView::activateVideoPipeline()
{
    if (videoPipelineActive)
        return;

    stopLogCapture();
    prepareVideoPipelineTick();
    brls::Application::setRenderSuspended(true);
    videoPipelineActive = true;
}

void StreamView::streamingTick()
{
    checkMenuTrigger();
    if (menuOpen)
    {
        brls::Application::setSwapInterval(1);
        app::ViewServices::current().platform.display().streaming(false);
        brls::Application::setExclusiveRender(false);
        brls::Application::setRenderSuspended(false);
        videoPipelineActive = false;
        return;
    }

    if (controls)
        controls->streaming();

    if (!session->MainLoop())
    {
        brls::Application::setSwapInterval(1);
        app::ViewServices::current().platform.display().streaming(false);
        brls::Application::setExclusiveRender(false);
        brls::Application::setRenderSuspended(false);
        brls::Application::setSuspendedRenderCallback(nullptr);
        brls::Application::setLimitedFPS(0);
        videoPipelineActive = false;
        intentionalDisconnect = true;
        brls::sync([weak = weak_from_this()]() {
            if (auto self = weak.lock())
                self->stopStream();
            stream_nav::unwindToBase();
        });
    }
}

void StreamView::onPlaying()
{
    std::string hostName = host->getHostName();
    log::info("Connected to {}", hostName);
    flow.connected();
#if AKIRA_CLOUD
    if (cloudScreen)
        cloudScreen->playing();
#endif
    brls::Application::notify(brls::getStr("akira/stream/connected_to", hostName));
}

#if AKIRA_CLOUD
void StreamView::drawCloudLaunch(NVGcontext* vg, float x, float y, float width, float height)
{
    cloudScreen->draw(vg, x, y, width, height);
    if (!streamActive || !sessionStarted)
        return;
    checkMenuTrigger();
    if (menuOpen)
        return;
    pollCloudCancel();
    if (sessionStarted)
        session->MainLoop();
}

void StreamView::pollCloudCancel()
{
    PadState pad;
    padInitializeDefault(&pad);
    padUpdate(&pad);
    const bool held = (padGetButtons(&pad) & HidNpadButton_B) != 0;
    const bool pressed = held && !cloudCancelHeld;
    cloudCancelHeld = held;
    if (!pressed)
        return;
    log::info("Cloud launch cancelled from the launch screen");
    disconnectWithSleep(false);
}

void StreamView::showCloudFailure(const std::string& message, bool retry)
{
    std::optional<cloud::CloudLaunch> again;
    if (retry && host->cloudLaunch())
        again = *host->cloudLaunch();

    auto* dialog = new brls::Dialog(brls::getStr("akira/cloud/start_stopped", message));
    dialog->setCancelable(false);
    dialog->addButton("akira/cloud/start_back"_i18n, []() {
        stream_nav::unwindToBase();
    });
    if (again)
    {
        dialog->addButton("akira/cloud/start_retry"_i18n, [again = std::move(*again)]() {
            stream_nav::unwindToBase();
            brls::sync([again]() {
                views::startConnecting(cloud::CloudConnectTask::retrying(app::ViewServices::current().cloud,
                    app::ViewServices::current().accounts, app::ViewServices::current().appSettings,
                    app::ViewServices::current().dispatcher, app::ViewServices::current().strings, again));
            });
        });
    }
    brls::Application::forceUnblockInputs();
    dialog->open();
}
#endif

void StreamView::onLaunchProgress(const libchiaking::play::LaunchProgress& progress)
{
    log::info("Launch progress: phase {} activity {}", static_cast<int>(progress.phase()),
                       progress.activity() ? static_cast<int>(*progress.activity()) : -1);
#if AKIRA_CLOUD
    if (cloudScreen)
        cloudScreen->progress(progress);
#endif
}

void StreamView::onConnectProgress(const libchiaking::play::ConnectProgress& progress)
{
    log::info("Connect progress: step {} attempt {}{}", static_cast<int>(progress.step), progress.attempt,
                       progress.reconnecting ? " (reconnecting)" : "");
    flow.follow(progress.step, progress.reconnecting);
}

void StreamView::onRefused(const libchiaking::play::StartRefusal& refusal)
{
    if (!sessionStarted)
        return;

    std::string text = brls::getStr(ConnectFailure::forRefusal(refusal.kind()));
    if (refusal.launch() && host->cloudLaunch())
        text = cloud::CloudLaunch::refusalText(app::ViewServices::current().strings, *refusal.launch(), host->cloudLaunch()->name());

    stopStream();
    app::views::SharedViewHolder::release(this);

#if AKIRA_CLOUD
    if (cloudScreen)
    {
        cloudScreen->failed();
        showCloudFailure(text, true);
        return;
    }
#endif

    auto* dialog = new brls::Dialog(brls::getStr("akira/stream/connection_failed", text));
    dialog->addButton("OK", []() {
        stream_nav::unwindToBase();
    });
    brls::Application::forceUnblockInputs();
    dialog->open();
}

void StreamView::onEnded(const libchiaking::play::Outcome& outcome)
{
    log::info("Session ended: ending={} cause={}", static_cast<int>(outcome.ending()),
                       outcome.cause() ? static_cast<int>(*outcome.cause()) : -1);

    streamActive = false;

    if (!sessionStarted) {
        log::info("onEnded: session already stopped via menu, skipping");
        return;
    }

    bool showsCloudScreen = false;
#if AKIRA_CLOUD
    showsCloudScreen = cloudScreen.has_value();
#endif
    const SessionEnding ending(outcome, SessionEnding::Situation{
        .loginPinEntered = loginPinEnteredThisSession,
        .duringConnect = !videoPipelineActive,
        .consoleAwake = host->isAwake(),
        .cloudScreen = showsCloudScreen,
        .cloudLaunch = host->cloudLaunch() != nullptr,
    });
    const SessionEnding::Verdict verdict = ending.verdict();
    const std::string reasonStr = brls::getStr(ending.reasonKey());

    if (verdict == SessionEnding::Verdict::Reconnect) {
        log::info("onEnded: first-time login PIN was entered, auto-reconnecting");
        loginPinEnteredThisSession = false;
        attemptReconnect();
        return;
    }

    stopStream();

    app::views::SharedViewHolder::release(this);

    if (verdict == SessionEnding::Verdict::Unpaired) {
        auto* dialog = new brls::Dialog("akira/stream/ps4_console_unregistered"_i18n);
        dialog->setCloseCallback([]() {
            stream_nav::unwindToBase();
        });
        dialog->addButton("OK", []() {
            stream_nav::unwindToBase();
        });
        brls::Application::forceUnblockInputs();
        dialog->open();
    } else if (verdict == SessionEnding::Verdict::Stopped) {
        brls::Application::notify("akira/stream/quit_stopped"_i18n);
        stream_nav::unwindToBase();
#if AKIRA_CLOUD
    } else if (verdict == SessionEnding::Verdict::CloudFailure) {
        cloudScreen->failed();
        std::string message = ending.duringConnect()
            ? brls::getStr(ending.failureKey())
            : brls::getStr("akira/stream/session_ended", reasonStr);
        if (outcome.launch_refused())
            message = cloud::CloudLaunch::refusalText(app::ViewServices::current().strings, *outcome.launch_refused(), host->cloudLaunch()->name());
        showCloudFailure(message, !ending.endsPlayTime());
#endif
    } else if (verdict == SessionEnding::Verdict::LaunchRefused) {
        auto* dialog = new brls::Dialog(brls::getStr("akira/stream/connection_failed",
            cloud::CloudLaunch::refusalText(app::ViewServices::current().strings, *outcome.launch_refused(), host->cloudLaunch()->name())));
        dialog->setCloseCallback([]() {
            stream_nav::unwindToBase();
        });
        dialog->addButton("OK", []() {
            stream_nav::unwindToBase();
        });
        brls::Application::forceUnblockInputs();
        dialog->open();
    } else {
        std::string body = ending.duringConnect()
            ? brls::getStr("akira/connection/connect_failed_title", brls::getStr(ending.connectFailureKey()))
            : brls::getStr("akira/stream/session_ended", reasonStr);
        auto* dialog = new brls::Dialog(body);
        dialog->setCloseCallback([]() {
            stream_nav::unwindToBase();
        });
        dialog->addButton("OK", []() {
            stream_nav::unwindToBase();
        });
        brls::Application::forceUnblockInputs();
        dialog->open();
    }
}

void StreamView::pauseStreamForUi()
{
    capturePausedFrame();
    menuOpen = true;
    if (session)
        session->setVideoPaused(true);
    brls::Application::forceUnblockInputs();
}

void StreamView::resumeStreamAfterUi()
{
    if (session)
        session->setVideoPaused(false);
    menuOpen = false;
    brls::Application::blockInputs(true);
}


void StreamView::onSeatPasscodeNeeded(uint8_t slot, bool retry)
{
    log::info("Couch passcode request for player {} (retry: {})", slot + 1, retry);

    if (couchPasscodeDialogOpen) {
        log::info("Couch passcode dialog already open, ignoring repeat request");
        return;
    }
    couchPasscodeDialogOpen = true;

    auto weak = weak_from_this();
    auto* pinView = new consoles::views::EnterPinView(host, consoles::views::PinViewType::CouchPasscode, retry);

    pinView->setOnPinEntered([weak, slot](const std::string& pin) {
        log::info("Couch passcode entered for player {}", slot + 1);
        if (auto self = weak.lock()) {
            self->couchPasscodeDialogOpen = false;
            if (self->connection)
                self->connection->sendSeatPasscode(slot, pin);
        }
    });

    pinView->setOnCancel([weak, slot]() {
        log::info("Couch passcode cancelled for player {}, releasing the pad", slot + 1);
        if (auto self = weak.lock()) {
            self->couchPasscodeDialogOpen = false;
            if (self->connection)
                self->connection->removePlayer(slot);
        }
    });

    brls::Application::pushActivity(new brls::Activity(pinView));
}

void StreamView::onSeatRefused(uint8_t slot, const libchiaking::play::SeatRefusal& refusal)
{
    const uint8_t status = ConnectFailure::seatStatus(refusal);
    log::error("Couch join failed for player {} with status {}", slot + 1, status);

    brls::Application::notify(status == 1
        ? brls::getStr("akira/couch/join_failed_not_signed_in", slot + 1)
        : brls::getStr("akira/couch/join_failed_status", slot + 1, status));
}

void StreamView::onSeatDropped(uint8_t slot)
{
    log::info("Couch: console dropped player {}, withdrawing the pad", slot + 1);
    brls::Application::notify(brls::getStr("akira/couch/join_failed_status", slot + 1,
        libchiaking::play::SeatRefusal::passcode_attempts_status));
}

void StreamView::onLoginPinNeeded(bool pinIncorrect)
{
    log::info("Login PIN request (incorrect: {})", pinIncorrect);

    const std::string savedPin = host->getConsolePIN();
    switch (StreamRecovery::loginPin(host->isCloud(), pinIncorrect, savedPin, connection != nullptr)) {
        case StreamRecovery::LoginPinStep::Ignore:
            log::warning("Ignoring login PIN request for a cloud session");
            return;
        case StreamRecovery::LoginPinStep::SubmitSaved:
            log::info("Using saved Console PIN for auto-login");
            connection->submitLoginPin(savedPin);
            return;
        case StreamRecovery::LoginPinStep::Ask:
            break;
    }

    consoles::Host* hostPtr = host;
    settings::SettingsStore* storePtr = &services->store;
    auto weak = weak_from_this();

    auto* pinView = new consoles::views::EnterPinView(hostPtr, consoles::views::PinViewType::Login, pinIncorrect);

    pinView->setOnPinEntered([weak, hostPtr, storePtr](const std::string& pin) {
        log::info("Login PIN entered, sending to session");
        if (auto self = weak.lock()) {
            if (self->connection)
                self->connection->submitLoginPin(pin);
            self->loginPinEnteredThisSession = true;
        }

        hostPtr->setConsolePIN(pin);
        storePtr->save();
    });

    pinView->setOnCancel([weak]() {
        log::info("Login PIN cancelled, stopping session");
        if (auto self = weak.lock()) {
            if (self->connection)
                self->connection->leave(false);
        }
    });

    brls::Application::pushActivity(new brls::Activity(pinView));
}

void StreamView::checkMenuTrigger()
{
    bool minusPressed = false;

    auto* input = session ? session->getInputManager() : nullptr;
    if (input != nullptr) {
        minusPressed = input->menuHeld();
    } else {
        PadState pad;
        padInitializeDefault(&pad);
        padUpdate(&pad);
        minusPressed = (padGetButtons(&pad) & HidNpadButton_Minus) != 0;
    }

    static int checkCount = 0;
    const auto now = MenuHold::Clock::now();
    if (minusPressed && checkCount++ % 30 == 0) {
        log::info("checkMenuTrigger: minus pressed, wasHeld={}, elapsed={}s", menuHold.held(),
                  menuHold.heldFor(now).count());
    }

    if (menuHold.update(minusPressed, now)) {
        log::info("checkMenuTrigger: 3 seconds reached, showing menu");
        showDisconnectMenu();
    }
}

void StreamView::showDisconnectMenu()
{
    log::info("showDisconnectMenu: entering");
    capturePausedFrame();

    if (session)
    {
        akira::platform::VideoRenderer* renderer = session->getVideoRenderer();
        if (renderer && renderer->takeOverlayPositionDirty())
            app::ViewServices::current().store.save();
    }
    menuOpen = true;
    if (connection)
        connection->setInputSuppressed(true);
    session->setVideoPaused(true);
    session->CleanUpHaptic();
    brls::Application::forceUnblockInputs();

    auto* menu = new StreamMenu();
    menu->setSleepAvailable(!host->isCloud());

    menu->setConsoleName(host->getHostName());
    menu->setConsoleIsPs5(host->isPS5());
    menu->setStatsMode(session->getStatsOverlayMode());

    auto weak = weak_from_this();

    menu->setOnStatsToggle([weak](StatsOverlayMode mode) {
        if (auto self = weak.lock()) {
            log::info("Stats overlay mode: {}", (int)mode);
            self->session->setStatsOverlayMode(mode);
        }
    });

    menu->setOnGyroReset([weak]() {
        if (auto self = weak.lock()) {
            log::info("Gyro reset triggered from menu");
            if (self->session->getInputManager()) {
                self->session->getInputManager()->resetMotionControls();
            }
        }
    });

    menu->setOnButtonMapping([]() {
        auto* remapView = new ControllerRemapView();
        remapView->setTranslucent(true);
        remapView->setStreamMode(true);
        brls::Application::pushActivity(new brls::Activity(remapView), brls::TransitionAnimation::NONE);
    });

    if (controls)
        controls->decorateMenu(*menu);

    menu->setOnDisconnect([weak](bool sleep) {
        if (auto self = weak.lock()) {
            log::info("Disconnect requested, sleep={}", sleep);
            self->disconnectWithSleep(sleep);
        }
    });

    menu->setOnDismiss([weak]() {
        if (auto self = weak.lock()) {
            log::info("Menu dismissed");
            self->session->setVideoPaused(false);
            self->menuOpen = false;
            if (self->connection)
                self->connection->setInputSuppressed(false);
            brls::Application::blockInputs(true);
        }
    });

    brls::Application::pushActivity(new brls::Activity(menu));
    log::info("showDisconnectMenu: menu opened");
}

void StreamView::abandonStart()
{
    log::info("Pad choice abandoned before the stream started");

    intentionalDisconnect = true;
    controllerReady       = false;

    if (session)
        session->FreeController();

    brls::Application::forceUnblockInputs();

    app::views::SharedViewHolder::release(this);

    stream_nav::unwindToBase();
}




bool StreamView::prepareController()
{
    if (controllerReady)
        return true;
    if (!session->InitController())
        return false;
    controllerReady = true;
    return true;
}
void StreamView::disconnectWithSleep(bool sleep)
{
    intentionalDisconnect = true;
    menuOpen = false;
    brls::Application::blockInputs(true);

    if (connection)
        connection->leave(sleep && !host->isCloud());

    stopStream();

    // Release early to invalidate weak_ptrs BEFORE activities are popped
    // This ensures any pending brls::sync tasks from chiaki callbacks
    // will fail weak.lock() and not access this object
    app::views::SharedViewHolder::release(this);

    brls::sync([]() { stream_nav::unwindToBase(); });
}

void StreamView::renderLogs(NVGcontext* vg, float x, float y, float width, float height)
{
    const std::vector<std::string> logLines = logTail.lines();

    nvgFontSize(vg, 16);
    nvgFillColor(vg, nvgRGBA(200, 200, 200, 255));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

    float lineHeight = 20;
    float startY = y + 20;

    // Show logs from bottom up (most recent at bottom)
    size_t startIdx = 0;
    size_t maxVisibleLines = static_cast<size_t>((height - 40) / lineHeight);
    if (logLines.size() > maxVisibleLines) {
        startIdx = logLines.size() - maxVisibleLines;
    }

    float currentY = startY;
    for (size_t i = startIdx; i < logLines.size(); i++) {
        nvgText(vg, x + 20, currentY, logLines[i].c_str(), nullptr);
        currentY += lineHeight;
    }
}

void StreamView::onFocusChanged(bool focused)
{
    if (!focused)
        return;

    if (!StreamRecovery::watchesFocus(intentionalDisconnect, services->networkSettings.autoReconnect(),
                                      sessionStarted, host->isCloud()))
        return;

    const bool sessionAlive = StreamRecovery::sessionAlive(connection ? connection->state() : std::nullopt);
    log::info("StreamView::onFocusChanged(InFocus): streamActive={}, sessionAlive={}",
                       streamActive, sessionAlive);

    if (StreamRecovery::lost(streamActive, sessionAlive)) {
        log::info("Stream dead on focus regain, prompting user");
        stopStream();
        brls::Application::forceUnblockInputs();

        auto weak = weak_from_this();
        auto* dialog = new brls::Dialog("akira/stream/disconnected_reconnect"_i18n);
        dialog->addButton("akira/stream/no"_i18n, [weak]() {
            if (auto self = weak.lock()) {
                app::views::SharedViewHolder::release(self.get());
            }
            stream_nav::unwindToBase();
        });
        dialog->addButton("akira/stream/yes"_i18n, [weak]() {
            if (auto self = weak.lock()) {
                self->attemptReconnect();
            }
        });
        dialog->open();
    }
}

void StreamView::attemptReconnect()
{
    if (reconnecting) {
        log::info("Already reconnecting, skipping");
        return;
    }

    reconnecting = true;

    log::info("Auto-reconnect: tearing down old session");
    stopStream();

    flow.restart();

    handover = libchiaking::play::ConsoleHandover::just_left;
    brls::Application::notify("akira/stream/reconnecting"_i18n);
    startStream();

    reconnecting = false;
}

}
