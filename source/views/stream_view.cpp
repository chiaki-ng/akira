#include "views/stream_view.hpp"
#include "views/stream_nav.hpp"
#include "views/controller_picker_view.hpp"
#include "views/players_panel_view.hpp"
#include "input/extended_input_manager.hpp"
#include "input/pad_path.hpp"
#include "views/stream_menu.hpp"
#include "views/connecting_view.hpp"
#include "cloud/cloud_connect_task.hpp"
#include "ui/theme.hpp"
#include "views/connection_stage.hpp"
#include "stream/video_renderer.hpp"
#include "views/enter_pin_view.hpp"
#include "views/controller_remap_view.hpp"
#include <format>
#include "core/exception.hpp"
#include "core/wireguard_manager.hpp"
#include "stream/input_manager.hpp"
#include "core/chiaki_core.hpp"
#include "util/shared_view_holder.hpp"
#include <switch.h>
#include <algorithm>
#include <chrono>
#include <borealis/core/i18n.hpp>
using namespace brls::literals;

namespace
{
    void boxBlurRGBA(std::vector<uint8_t>& pixels, int width, int height, int radius)
    {
        if (radius <= 0 || width <= 1 || height <= 1 ||
            pixels.size() != static_cast<size_t>(width) * height * 4)
            return;

        std::vector<uint8_t> scratch(pixels.size());
        const int diameter = radius * 2 + 1;

        for (int y = 0; y < height; y++)
        {
            int sums[4] = {};
            for (int dx = -radius; dx <= radius; dx++)
            {
                const int sx = std::clamp(dx, 0, width - 1);
                const size_t src = (static_cast<size_t>(y) * width + sx) * 4;
                for (int c = 0; c < 4; c++)
                    sums[c] += pixels[src + c];
            }

            for (int x = 0; x < width; x++)
            {
                const size_t dst = (static_cast<size_t>(y) * width + x) * 4;
                for (int c = 0; c < 4; c++)
                    scratch[dst + c] = static_cast<uint8_t>(sums[c] / diameter);

                const int removeX = std::clamp(x - radius, 0, width - 1);
                const int addX = std::clamp(x + radius + 1, 0, width - 1);
                const size_t remove = (static_cast<size_t>(y) * width + removeX) * 4;
                const size_t add = (static_cast<size_t>(y) * width + addX) * 4;
                for (int c = 0; c < 4; c++)
                    sums[c] += static_cast<int>(pixels[add + c]) - pixels[remove + c];
            }
        }

        for (int x = 0; x < width; x++)
        {
            int sums[4] = {};
            for (int dy = -radius; dy <= radius; dy++)
            {
                const int sy = std::clamp(dy, 0, height - 1);
                const size_t src = (static_cast<size_t>(sy) * width + x) * 4;
                for (int c = 0; c < 4; c++)
                    sums[c] += scratch[src + c];
            }

            for (int y = 0; y < height; y++)
            {
                const size_t dst = (static_cast<size_t>(y) * width + x) * 4;
                for (int c = 0; c < 4; c++)
                    pixels[dst + c] = static_cast<uint8_t>(sums[c] / diameter);

                const int removeY = std::clamp(y - radius, 0, height - 1);
                const int addY = std::clamp(y + radius + 1, 0, height - 1);
                const size_t remove = (static_cast<size_t>(removeY) * width + x) * 4;
                const size_t add = (static_cast<size_t>(addY) * width + x) * 4;
                for (int c = 0; c < 4; c++)
                    sums[c] += static_cast<int>(scratch[add + c]) - scratch[remove + c];
            }
        }
    }

    void blurPausedFrame(std::vector<uint8_t>& pixels, int width, int height)
    {
        // The menu frame is already captured at 640 px wide. Three small box
        // passes approximate a soft Gaussian blur while keeping this one-time
        // transition comfortably cheaper than a live framebuffer effect.
        constexpr int kPassRadius = 5;
        for (int pass = 0; pass < 3; pass++)
            boxBlurRGBA(pixels, width, height, kPassRadius);
    }
}



StreamView::StreamView(Host* host, std::shared_ptr<Host> hostOwner)
    : host(host)
    , hostOwner(std::move(hostOwner))
{
    this->session = Session::GetInstance();
    this->settings = SettingsManager::getInstance();
    if (const auto* launch = host->cloudLaunch())
        cloudScreen.emplace(launch->name());

    setFocusable(true);
}

void StreamView::setupCallbacks()
{
    auto weak = weak_from_this();

    session->getInputManager()->setTargetPS5(host->isPS5());

    session->getInputManager()->setOnPadArrived([weak](HidNpadIdType npad) {
        brls::sync([weak, npad]() {
            if (auto self = weak.lock())
                self->onPadArrived(npad);
        });
    });

    session->getInputManager()->setOnCouchDisconnected([weak](HidNpadIdType, uint8_t slot) {
        brls::sync([weak, slot]() {
            if (auto self = weak.lock()) {
                if (self->connection)
                    self->connection->removePlayer(slot);
            }
        });
    });

    exitSubscription = brls::Application::getExitEvent()->subscribe([weak]() {
        if (auto self = weak.lock()) {
            if (self->sessionStarted && self->settings->getSleepOnExit() && !self->host->isCloud() && self->connection) {
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
    if (logSubscriptionActive)
        return;

    auto weak = weak_from_this();
    logSubscription = brls::Logger::subscribeToLog(
        [weak](brls::Logger::TimePoint, brls::LogLevel, std::string msg) {
            if (auto self = weak.lock()) {
                std::lock_guard<std::mutex> lock(self->logMutex);
                self->logLines.push_back(msg);
                while (self->logLines.size() > MAX_LOG_LINES) {
                    self->logLines.pop_front();
                }
            }
        });
    logSubscriptionActive = true;
}

void StreamView::stopLogCapture()
{
    if (!logSubscriptionActive)
        return;

    brls::Logger::unsubscribeFromLog(logSubscription);
    logSubscriptionActive = false;
}

void StreamView::adoptConnectionLog(akira::ConnectionLog log)
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
    SharedViewHolder::release(this);
}

brls::View* StreamView::create()
{
    // This requires a host to be passed, so we return nullptr for XML creation
    return nullptr;
}

bool StreamView::beginPadChoice()
{
    if (padChoiceDone)
        return false;

    if (session == nullptr) {
        padChoiceDone = true;
        return false;
    }

    if (!controllerReady) {
        if (!session->InitController()) {
            padChoiceDone = true;
            return false;
        }
        controllerReady = true;
    }

    waitForPads(0);
    return true;
}

void StreamView::waitForPads(int attempt)
{
    constexpr int kMaxAttempts = 20;
    constexpr int kIntervalMs  = 100;

    auto* input = session ? session->getInputManager() : nullptr;
    if (input == nullptr) {
        finishPadChoice();
        return;
    }

    const auto availability = input->extendedInput().availability();
    const bool backendUsable =
        availability == ExtendedInputManager::Availability::Available;

    if (!backendUsable) {
        brls::Logger::info("PadChoice: no backend to wait for, deciding now");
        finishPadChoice();
        return;
    }

    for (const auto& pad : input->describePads()) {
        if (pad.kind == akira::input::PadPathKind::McPsNative ||
            pad.kind == akira::input::PadPathKind::McGeneric) {
            brls::Logger::info("PadChoice: backend named a pad after {}ms",
                               attempt * kIntervalMs);
            finishPadChoice();
            return;
        }
    }

    if (attempt >= kMaxAttempts) {
        brls::Logger::info("PadChoice: no backend pad after {}ms, deciding anyway",
                           attempt * kIntervalMs);
        finishPadChoice();
        return;
    }

    auto weak = weak_from_this();
    brls::delay(kIntervalMs, [weak, attempt]() {
        if (auto self = weak.lock())
            self->waitForPads(attempt + 1);
    });
}

void StreamView::finishPadChoice()
{
    padChoiceDone = true;

    auto* input = session ? session->getInputManager() : nullptr;
    if (input == nullptr) {
        startStream();
        return;
    }

    auto pads = input->describePads();

    {
        AkiraInputDeviceList devices{};
        if (input->extendedInput().listDevices(&devices)) {
            brls::Logger::info("PadChoice: backend lists {} device(s)", devices.count);
            for (uint8_t i = 0; i < devices.count && i < AKIRA_INPUT_MAX_LISTED_DEVICES; i++) {
                const AkiraInputDeviceInfo& d = devices.devices[i];
                brls::Logger::info(
                    "PadChoice:   {:04x}:{:04x} report=0x{:02x} flags=0x{:02x}{}{}{}",
                    d.vendor_id, d.product_id, d.report_id, d.flags,
                    (d.flags & AkiraInputDevice_Identified) ? " identified" : "",
                    (d.flags & AkiraInputDevice_Reporting)  ? " reporting"  : "",
                    (d.flags & AkiraInputDevice_Claimed)    ? " claimed"    : "");
            }
        } else {
            brls::Logger::warning("PadChoice: listDevices failed");
        }
    }

    for (const auto& pad : pads) {
        brls::Logger::info("PadChoice: npad {} {} [{}{}]",
                           (int)pad.npad, pad.label,
                           pad.caps.analog_triggers ? "analog " : "",
                           pad.caps.touchpad ? "touch " : "");
    }

    if (!ControllerPickerView::worthAsking(pads)) {
        brls::Logger::info("PadChoice: {} pad(s), nothing worth asking", pads.size());
        for (const auto& pad : pads) {
            if (pad.caps.analog_triggers || pad.caps.touchpad) {
                input->selectNpad(pad.npad);
                break;
            }
        }
        startStream();
        return;
    }

    brls::Logger::info("PadChoice: asking between {} pads", pads.size());

    auto weak = weak_from_this();
    auto describe = [weak]() -> std::vector<akira::input::PadDescription> {
        if (auto self = weak.lock()) {
            if (auto* mgr = self->session ? self->session->getInputManager() : nullptr)
                return mgr->describePads();
        }
        return {};
    };

    auto* picker = new ControllerPickerView(pads, [weak](HidNpadIdType npad) {
        auto self = weak.lock();

        if (npad == ControllerPickerView::kCancelled) {
            if (self)
                self->abandonBeforeStart();
            else
                brls::Application::popActivity();
            return;
        }

        brls::Application::popActivity();

        if (!self)
            return;

        if (npad != ControllerPickerView::kNoChoice) {
            if (auto* mgr = self->session ? self->session->getInputManager() : nullptr)
                mgr->selectNpad(npad);
        }
        self->startStream();
    }, describe);

    brls::Application::pushActivity(new brls::Activity(picker),
                                    brls::TransitionAnimation::NONE);
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

    if (beginPadChoice())
    {
        return;
    }

    if (sessionPreStarted)
    {
        brls::Logger::info("Session already started while connecting, activating stream...");
        sessionStarted = true;
        streamActive = true;
        prepareVideoPipelineTick();
        brls::Application::blockInputs(true);
        brls::Logger::info("Stream activated successfully");
        return;
    }

    brls::Logger::info("Starting stream to {}", host->getHostName());

    try
    {
        auto profile = SettingsManager::StreamProfile::Local;
        if (host->isCloud()) {
            profile = SettingsManager::StreamProfile::Cloud;
            settings->setActiveCloudPscloud(host->cloudLaunch()->ps5());
        }
        else if (host->isRemote())
            profile = SettingsManager::StreamProfile::Remote;
        else if (WireGuardManager::instance().isConnected())
            profile = SettingsManager::StreamProfile::Vpn;
        settings->setActiveStreamProfile(profile);

        if (!controllerReady)
        {
            if (!session->InitController())
            {
                brls::Logger::error("Failed to initialize controller");
                throw Exception("akira/stream/failed_init_controller"_i18n);
            }
            controllerReady = true;
        }

        openConnection();

        sessionStarted = true;
        streamActive = true;
        prepareVideoPipelineTick();

        brls::Application::blockInputs(true);

        brls::Logger::info("Stream started successfully");
    }
    catch (const Exception& e)
    {
        brls::Logger::error("Failed to start stream: {}", e.what());

        connection.reset();
        session->FreeController();
        session->FreeVideo();

        std::string errorMsg = e.what();
        brls::sync([errorMsg]() {
            auto* dialog = new brls::Dialog(brls::getStr("akira/stream/connection_failed", errorMsg));
            dialog->addButton("OK", []() {
                akira::views::stream_nav::unwindToBase();
            });
            dialog->open();
            brls::Application::forceUnblockInputs();
        });
    }
}

akira::input::RumbleProfile StreamView::activePadProfile()
{
    auto* settings = SettingsManager::getInstance();
    const std::vector<akira::input::PadDescription> pads = akira::input::DescribePads();

    if (pads.empty())
        return settings->getRumbleProfile(akira::input::kRumbleKeyDefault);

    const akira::input::PadDescription& pad = pads.front();
    const bool switchNative = pad.kind == akira::input::PadPathKind::JoyCon
                           || pad.kind == akira::input::PadPathKind::SwitchPro;

    return settings->resolveRumbleProfile(pad.vendor_id, pad.product_id,
                                          pad.has_address ? pad.bt_addr : nullptr,
                                          switchNative,
                                          pad.kind == akira::input::PadPathKind::JoyCon);
}

void StreamView::openConnection()
{
    using akira::stream::Connection;
    namespace play = libchiaking::play;

    auto* accounts = akira::ChiakiCore::current().accounts();
    auto player = accounts ? accounts->activeUser() : nullptr;
    const auto& console = host->getConsole();
    Connection::Starter starter;
    int width = 0;
    int height = 0;
    int fps = 60;
    int bitrate = 0;
    bool hevc = false;
    if (const auto* launch = host->cloudLaunch()) {
        auto preferences = Connection::cloudPreferencesFor(*launch, *settings);
        if (!preferences)
            throw Exception("akira/connection/fail_generic"_i18n);
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
            throw Exception("akira/connection/fail_regist"_i18n);
        auto preferences = Connection::preferencesFor(*host, *settings);
        if (!preferences)
            throw Exception("akira/connection/fail_generic"_i18n);
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

    brls::Logger::info("StreamView: requesting {}x{} at {} fps, {} kbps, {}", width, height, fps, bitrate,
                       hevc ? "HEVC" : "H.264");
    session->setRequestedProfile(width, height, fps, bitrate, hevc);

    const akira::input::RumbleProfile padProfile = activePadProfile();
    session->setRumbleStrength(padProfile.rumble_source == akira::input::RumbleSource::Off
                                   ? 0.0f
                                   : padProfile.strength);
    session->setRumbleSource(padProfile.rumble_source);
    session->setHapticBase(akira::input::HapticBaseForIntensity(padProfile.haptic_intensity));
    session->setRumbleFreqs(padProfile.freq_low, padProfile.freq_high);
    session->setEnvelopeAttack(padProfile.envelope_attack);
    session->setEnvelopeDecay(padProfile.envelope_decay);
    session->setRumbleCeiling(padProfile.ceiling);

    if (!session->InitAVCodec(hevc, width, height))
        throw Exception("Failed to initiate libav codec");
    if (!session->InitVideo(width, height))
        throw Exception("Failed to initiate video");

    const char* platformLabel = host->isPS5() ? "PS5" : "PS4";
    if (const auto* cloud = host->cloudLaunch()) {
        session->setConnectionContext(std::format("Cloud \xc2\xb7 {}", cloud->name()));
    } else if (host->isRemote()) {
        session->setConnectionContext(std::format("{} \xc2\xb7 Remote", platformLabel));
    } else if (WireGuardManager::instance().isConnected()) {
        session->setConnectionContext(std::format("{} \xc2\xb7 VPN", platformLabel));
    } else {
        session->setConnectionContext(std::format("{} \xc2\xb7 Direct", platformLabel));
    }

    std::shared_ptr<akira::stream::ConnectionListener> self = shared_from_this();
    connection = std::make_shared<Connection>(*session, self, host->isPS5());
    if (!connection->open(std::move(starter))) {
        connection.reset();
        throw Exception("akira/connection/failed_create_thread"_i18n);
    }
}

void StreamView::stopStream()
{
    if (!sessionStarted)
    {
        return;
    }

    brls::Logger::info("Stopping stream");

    brls::Application::forceUnblockInputs();
    brls::Application::setRenderSuspended(false);
    if (session)
    brls::Application::setSuspendedRenderCallback(nullptr);
    brls::Application::setLimitedFPS(0);
    brls::Application::setSwapInterval(1);
    brls::Application::setExclusiveRender(false);
    videoPipelineActive = false;

    streamActive = false;

    settings->setStreamingActive(false);
    connection.reset();

    session->FreeController();
    session->FreeVideo();

    controllerReady = false;

    sessionStarted = false;
}

void StreamView::capturePausedFrame()
{
    pausedFrameRGBA.clear();
    pausedFrameW = 0;
    pausedFrameH = 0;

    if (!session)
        return;

    IVideoRenderer* renderer = session->getVideoRenderer();
    if (!renderer)
        return;

    if (!renderer->captureLastFrame(pausedFrameRGBA, pausedFrameW, pausedFrameH))
    {
        pausedFrameRGBA.clear();
        pausedFrameW = 0;
        pausedFrameH = 0;
        return;
    }

    blurPausedFrame(pausedFrameRGBA, pausedFrameW, pausedFrameH);
}

void StreamView::releasePausedFrame(NVGcontext* vg)
{
    if (pausedFrameImage >= 0 && vg)
        nvgDeleteImage(vg, pausedFrameImage);
    pausedFrameImage = -1;
    pausedFrameRGBA.clear();
    pausedFrameRGBA.shrink_to_fit();
    pausedFrameW = 0;
    pausedFrameH = 0;
}

bool StreamView::drawPausedFrame(NVGcontext* vg, float x, float y, float width, float height)
{
    if (pausedFrameImage < 0 && !pausedFrameRGBA.empty() && pausedFrameW > 0 && pausedFrameH > 0)
    {
        pausedFrameImage = nvgCreateImageRGBA(vg, pausedFrameW, pausedFrameH, 0, pausedFrameRGBA.data());
        if (pausedFrameImage < 0)
            brls::Logger::warning("StreamView: failed to create paused frame image");
        pausedFrameRGBA.clear();
        pausedFrameRGBA.shrink_to_fit();
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
    static int drawCount = 0;
    ++drawCount;
    if (settings->getDebugRenderLog() && drawCount % 60 == 0) {
        brls::Logger::info("StreamView::draw #{}: streamActive={}, sessionStarted={}, menuOpen={}",
            drawCount, streamActive, sessionStarted, menuOpen);
    }

    if (cloudScreen && (!streamActive || !sessionStarted || !session->hasReceivedFirstFrame()))
    {
        drawCloudLaunch(vg, x, y, width, height);
        return;
    }

    if (!streamActive || !sessionStarted)
    {
        nvgBeginPath(vg);
        nvgRect(vg, x, y, width, height);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
        nvgFill(vg);

        if (settings->getConnectionShowStages()) {
            ConnectionStage stage = static_cast<ConnectionStage>(currentStage.load());
            int raw = static_cast<int>(stage);
            int idx = raw < 1 ? 1 : (raw > 6 ? 6 : raw);
            drawConnectionRing(vg, x + width / 2, y + height / 2,
                brls::getStr(connectionStageLabelKey(stage)), idx, 6);
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

    static bool wasMenuOpen = false;
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
        brls::Logger::info("StreamView::draw: resuming after menu close, stats_overlay={}",
            (int)session->getStatsOverlayMode());
        wasMenuOpen = false;
        releasePausedFrame(vg);

        nvgBeginPath(vg);
        nvgRect(vg, x, y, width, height);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
        nvgFill(vg);
        brls::Logger::info("StreamView::draw: skipping first frame after menu");
        return;
    }

    if (!session->hasReceivedFirstFrame())
    {
        nvgBeginPath(vg);
        nvgRect(vg, x, y, width, height);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
        nvgFill(vg);

        if (settings->getConnectionShowStages()) {
            ConnectionStage stage = static_cast<ConnectionStage>(currentStage.load());
            int raw = static_cast<int>(stage);
            int idx = raw < 1 ? 1 : (raw > 6 ? 6 : raw);
            drawConnectionRing(vg, x + width / 2, y + height / 2,
                brls::getStr(connectionStageLabelKey(stage)), idx, 6);
        } else {
            renderLogs(vg, x, y, width, height);
        }

        session->MainLoop();
        return;
    }

    activateVideoPipeline();
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
        brls::Application::setExclusiveRender(false);
        brls::Application::setRenderSuspended(false);
        videoPipelineActive = false;
        return;
    }

    if (auto* input = session->getInputManager())
    {
        if (input->extendedInput().consumeDegradedNotice())
        {
            brls::sync([]() {
                brls::Application::notify("akira/settings/analog_triggers_degraded"_i18n);
            });
        }
    }

    if (!session->MainLoop())
    {
        brls::Application::setSwapInterval(1);
        brls::Application::setExclusiveRender(false);
        brls::Application::setRenderSuspended(false);
        brls::Application::setSuspendedRenderCallback(nullptr);
        brls::Application::setLimitedFPS(0);
        videoPipelineActive = false;
        intentionalDisconnect = true;
        brls::sync([weak = weak_from_this()]() {
            if (auto self = weak.lock())
                self->stopStream();
            akira::views::stream_nav::unwindToBase();
        });
    }
}

void StreamView::onPlaying()
{
    std::string hostName = host->getHostName();
    brls::Logger::info("Connected to {}", hostName);
    currentStage = static_cast<int>(ConnectionStage::Connected);
    if (cloudScreen)
        cloudScreen->playing();
    brls::Application::notify(brls::getStr("akira/stream/connected_to", hostName));
}

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
    brls::Logger::info("Cloud launch cancelled from the launch screen");
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
        akira::views::stream_nav::unwindToBase();
    });
    if (again)
    {
        dialog->addButton("akira/cloud/start_retry"_i18n, [again = std::move(*again)]() {
            akira::views::stream_nav::unwindToBase();
            brls::sync([again]() {
                akira::views::startConnecting(cloud::CloudConnectTask::retrying(again));
            });
        });
    }
    brls::Application::forceUnblockInputs();
    dialog->open();
}

void StreamView::onLaunchProgress(const libchiaking::play::LaunchProgress& progress)
{
    brls::Logger::info("Launch progress: phase {} activity {}", static_cast<int>(progress.phase()),
                       progress.activity() ? static_cast<int>(*progress.activity()) : -1);
    if (cloudScreen)
        cloudScreen->progress(progress);
}

void StreamView::onConnectProgress(const libchiaking::play::ConnectProgress& progress)
{
    brls::Logger::info("Connect progress: step {} attempt {}{}", static_cast<int>(progress.step), progress.attempt,
                       progress.reconnecting ? " (reconnecting)" : "");
    const int stage = static_cast<int>(connectionStageOf(progress.step));
    if (progress.reconnecting || stage > currentStage.load())
        currentStage = stage;
}

void StreamView::onRefused(const libchiaking::play::StartRefusal& refusal)
{
    using Kind = libchiaking::play::StartRefusal::Kind;
    if (!sessionStarted)
        return;

    const char* key = "akira/connection/fail_generic";
    switch (refusal.kind()) {
        case Kind::needs_psn_sign_in:
        case Kind::sign_in_expired: key = "akira/stream/psn_token_expired"; break;
        case Kind::remote_play_disabled: key = "akira/connection/fail_rp_disabled"; break;
        case Kind::not_paired: key = "akira/connection/fail_regist"; break;
        case Kind::console_unreachable: key = "akira/connection/fail_unreachable"; break;
        case Kind::busy: key = "akira/connection/fail_in_use"; break;
        default: break;
    }

    std::string text = brls::getStr(key);
    if (refusal.launch() && host->cloudLaunch())
        text = cloud::CloudLaunch::refusalText(*refusal.launch(), host->cloudLaunch()->name());

    stopStream();
    SharedViewHolder::release(this);

    if (cloudScreen)
    {
        cloudScreen->failed();
        showCloudFailure(text, true);
        return;
    }

    auto* dialog = new brls::Dialog(brls::getStr("akira/stream/connection_failed", text));
    dialog->addButton("OK", []() {
        akira::views::stream_nav::unwindToBase();
    });
    brls::Application::forceUnblockInputs();
    dialog->open();
}

void StreamView::onEnded(const libchiaking::play::Outcome& outcome)
{
    using libchiaking::play::ConsoleQuit;
    using libchiaking::play::EndReason;

    brls::Logger::info("Session ended: ending={} cause={}", static_cast<int>(outcome.ending()),
                       outcome.cause() ? static_cast<int>(*outcome.cause()) : -1);

    streamActive = false;

    if (!sessionStarted) {
        brls::Logger::info("onEnded: session already stopped via menu, skipping");
        return;
    }

    const auto& quit = outcome.quit();
    const bool stoppedByConsole = quit && quit->reason() == ConsoleQuit::Reason::stopped;
    const bool stopped = outcome.was_left() || stoppedByConsole;
    const std::string reasonStr = quit
        ? brls::getStr(quitReasonKey(quit->reason()))
        : brls::getStr(connectionFailureKeyFor(outcome));
    const bool rejectedWhileAwake = host->isAwake() && quit &&
        quit->reason() == ConsoleQuit::Reason::session_request_unknown;
    const bool unpairedConsole = outcome.cause() == EndReason::Cause::unpaired_console;

    if (loginPinEnteredThisSession && !stopped && !unpairedConsole) {
        brls::Logger::info("onEnded: first-time login PIN was entered, auto-reconnecting");
        loginPinEnteredThisSession = false;
        attemptReconnect();
        return;
    }

    const bool duringConnect = !videoPipelineActive;

    stopStream();

    SharedViewHolder::release(this);

    if (unpairedConsole) {
        auto* dialog = new brls::Dialog("akira/stream/ps4_console_unregistered"_i18n);
        dialog->setCloseCallback([]() {
            akira::views::stream_nav::unwindToBase();
        });
        dialog->addButton("OK", []() {
            akira::views::stream_nav::unwindToBase();
        });
        brls::Application::forceUnblockInputs();
        dialog->open();
    } else if (stopped) {
        brls::Application::notify("akira/stream/quit_stopped"_i18n);
        akira::views::stream_nav::unwindToBase();
    } else if (cloudScreen) {
        cloudScreen->failed();
        const bool endsPlayTime = quit && (quit->reason() == ConsoleQuit::Reason::time_limit_reached ||
                                           quit->reason() == ConsoleQuit::Reason::trial_ended);
        std::string message = duringConnect
            ? brls::getStr(connectionFailureKeyFor(outcome))
            : brls::getStr("akira/stream/session_ended", reasonStr);
        if (outcome.launch_refused())
            message = cloud::CloudLaunch::refusalText(*outcome.launch_refused(), host->cloudLaunch()->name());
        showCloudFailure(message, !endsPlayTime);
    } else if (outcome.launch_refused() && host->cloudLaunch()) {
        auto* dialog = new brls::Dialog(brls::getStr("akira/stream/connection_failed",
            cloud::CloudLaunch::refusalText(*outcome.launch_refused(), host->cloudLaunch()->name())));
        dialog->setCloseCallback([]() {
            akira::views::stream_nav::unwindToBase();
        });
        dialog->addButton("OK", []() {
            akira::views::stream_nav::unwindToBase();
        });
        brls::Application::forceUnblockInputs();
        dialog->open();
    } else {
        std::string body = duringConnect
            ? brls::getStr("akira/connection/connect_failed_title",
                brls::getStr(rejectedWhileAwake
                    ? "akira/connection/fail_rp_disabled"
                    : connectionFailureKeyFor(outcome)))
            : brls::getStr("akira/stream/session_ended", reasonStr);
        auto* dialog = new brls::Dialog(body);
        dialog->setCloseCallback([]() {
            akira::views::stream_nav::unwindToBase();
        });
        dialog->addButton("OK", []() {
            akira::views::stream_nav::unwindToBase();
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

void StreamView::onPadArrived(HidNpadIdType npad)
{
    if (!connection || !connection->couchSlotAvailable()) {
        brls::Logger::info("Couch: pad arrived on npad {} but no slot is free", (int)npad);
        return;
    }

    if (couchArrivalPromptOpen || menuOpen) {
        brls::Logger::info("Couch: pad arrived on npad {} while UI is busy, not prompting", (int)npad);
        return;
    }
    couchArrivalPromptOpen = true;

    pauseStreamForUi();

    auto weak = weak_from_this();
    auto* dialog = new brls::Dialog("akira/couch/controller_arrived"_i18n);

    dialog->addButton("akira/couch/add_player"_i18n, [weak]() {
        auto self = weak.lock();
        if (!self)
            return;
        self->couchArrivalPromptOpen = false;
        self->openCouchClaim();
    });

    dialog->addButton("akira/couch/not_now"_i18n, [weak]() {
        auto self = weak.lock();
        if (!self)
            return;
        self->couchArrivalPromptOpen = false;
        auto* input = self->session ? self->session->getInputManager() : nullptr;
        if (input)
            input->resolvePadArrival();
        if (input && input->couchRoster().empty()) {
            self->openPrimaryControllerPicker();
            return;
        }
        self->resumeStreamAfterUi();
    });

    dialog->setCloseCallback([weak]() {
        auto self = weak.lock();
        if (!self)
            return;
        self->couchArrivalPromptOpen = false;
        if (auto* input = self->session ? self->session->getInputManager() : nullptr)
            input->resolvePadArrival();
        self->resumeStreamAfterUi();
    });

    brls::Logger::info("Couch: showing controller arrival prompt");
    dialog->open();
}

void StreamView::openCouchClaim()
{
    auto* input = session ? session->getInputManager() : nullptr;
    if (input == nullptr) {
        brls::Logger::warning("StreamView: no input manager for couch claim");
        resumeStreamAfterUi();
        return;
    }

    auto weak = weak_from_this();
    auto* panel = new PlayersPanelView(host, connection, input, true);
    panel->setOnClosed([weak]() {
        auto self = weak.lock();
        if (!self)
            return;
        if (auto* mgr = self->session ? self->session->getInputManager() : nullptr)
            mgr->resolvePadArrival();
        self->resumeStreamAfterUi();
    });
    brls::Application::pushActivity(new brls::Activity(panel), brls::TransitionAnimation::NONE);
}

void StreamView::openPrimaryControllerPicker()
{
    auto* input = session ? session->getInputManager() : nullptr;
    if (!input) {
        resumeStreamAfterUi();
        return;
    }

    auto pads = input->describePads();
    if (!ControllerPickerView::worthAsking(pads)) {
        brls::Logger::info("Couch: no alternate primary controller worth asking about");
        resumeStreamAfterUi();
        return;
    }

    brls::Logger::info("Couch: opening primary controller picker after Not now");
    auto weak = weak_from_this();
    auto describe = [weak]() -> std::vector<akira::input::PadDescription> {
        if (auto self = weak.lock()) {
            if (auto* mgr = self->session ? self->session->getInputManager() : nullptr)
                return mgr->describePads();
        }
        return {};
    };

    auto* picker = new ControllerPickerView(pads, [weak](HidNpadIdType npad) {
        brls::Application::popActivity(brls::TransitionAnimation::NONE);

        auto self = weak.lock();
        if (!self)
            return;
        if (npad != ControllerPickerView::kCancelled &&
            npad != ControllerPickerView::kNoChoice) {
            if (auto* mgr = self->session ? self->session->getInputManager() : nullptr)
                mgr->selectNpad(npad);
        }
        self->resumeStreamAfterUi();
    }, describe);

    brls::Application::pushActivity(new brls::Activity(picker),
                                    brls::TransitionAnimation::NONE);
}

void StreamView::onSeatPasscodeNeeded(uint8_t slot, bool retry)
{
    brls::Logger::info("Couch passcode request for player {} (retry: {})", slot + 1, retry);

    if (couchPasscodeDialogOpen) {
        brls::Logger::info("Couch passcode dialog already open, ignoring repeat request");
        return;
    }
    couchPasscodeDialogOpen = true;

    auto weak = weak_from_this();
    auto* pinView = new EnterPinView(host, PinViewType::CouchPasscode, retry);

    pinView->setOnPinEntered([weak, slot](const std::string& pin) {
        brls::Logger::info("Couch passcode entered for player {}", slot + 1);
        if (auto self = weak.lock()) {
            self->couchPasscodeDialogOpen = false;
            if (self->connection)
                self->connection->sendSeatPasscode(slot, pin);
        }
    });

    pinView->setOnCancel([weak, slot]() {
        brls::Logger::info("Couch passcode cancelled for player {}, releasing the pad", slot + 1);
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
    using Kind = libchiaking::play::SeatRefusal::Kind;
    uint8_t status = 0;
    switch (refusal.kind()) {
        case Kind::account_not_on_console: status = 1; break;
        case Kind::refused_by_console: status = refusal.console_status().value_or(0); break;
        case Kind::passcode_attempts_used: status = libchiaking::play::SeatRefusal::passcode_attempts_status; break;
        case Kind::timed_out: status = libchiaking::play::SeatRefusal::timed_out_status; break;
        default: break;
    }
    brls::Logger::error("Couch join failed for player {} with status {}", slot + 1, status);

    brls::Application::notify(status == 1
        ? brls::getStr("akira/couch/join_failed_not_signed_in", slot + 1)
        : brls::getStr("akira/couch/join_failed_status", slot + 1, status));
}

void StreamView::onSeatDropped(uint8_t slot)
{
    brls::Logger::info("Couch: console dropped player {}, withdrawing the pad", slot + 1);
    brls::Application::notify(brls::getStr("akira/couch/join_failed_status", slot + 1,
        libchiaking::play::SeatRefusal::passcode_attempts_status));
}

void StreamView::onLoginPinNeeded(bool pinIncorrect)
{
    brls::Logger::info("Login PIN request (incorrect: {})", pinIncorrect);

    if (host->isCloud()) {
        brls::Logger::warning("Ignoring login PIN request for a cloud session");
        return;
    }

    if (!pinIncorrect) {
        std::string savedPin = settings->getConsolePIN(host);
        if (!savedPin.empty() && connection) {
            brls::Logger::info("Using saved Console PIN for auto-login");
            connection->submitLoginPin(savedPin);
            return;
        }
    }

    Host* hostPtr = host;
    SettingsManager* settingsPtr = settings;
    auto weak = weak_from_this();

    auto* pinView = new EnterPinView(hostPtr, PinViewType::Login, pinIncorrect);

    pinView->setOnPinEntered([weak, hostPtr, settingsPtr](const std::string& pin) {
        brls::Logger::info("Login PIN entered, sending to session");
        if (auto self = weak.lock()) {
            if (self->connection)
                self->connection->submitLoginPin(pin);
            self->loginPinEnteredThisSession = true;
        }

        settingsPtr->setConsolePIN(hostPtr, pin);
        settingsPtr->writeFile();
    });

    pinView->setOnCancel([weak]() {
        brls::Logger::info("Login PIN cancelled, stopping session");
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
    if (minusPressed && checkCount++ % 30 == 0) {
        auto elapsed = minusWasHeld ?
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - minusHoldStart).count() : 0;
        brls::Logger::info("checkMenuTrigger: minus pressed, wasHeld={}, elapsed={}s", minusWasHeld, elapsed);
    }

    if (minusPressed) {
        if (!minusWasHeld) {
            minusHoldStart = std::chrono::steady_clock::now();
            minusWasHeld = true;
        } else {
            auto elapsed = std::chrono::steady_clock::now() - minusHoldStart;
            if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() >= 3) {
                brls::Logger::info("checkMenuTrigger: 3 seconds reached, showing menu");
                showDisconnectMenu();
                minusWasHeld = false;  // Reset
            }
        }
    } else {
        minusWasHeld = false;
    }
}

void StreamView::showDisconnectMenu()
{
    brls::Logger::info("showDisconnectMenu: entering");
    capturePausedFrame();

    if (session)
    {
        IVideoRenderer* renderer = session->getVideoRenderer();
        if (renderer && renderer->takeOverlayPositionDirty())
            SettingsManager::getInstance()->writeFile();
    }
    menuOpen = true;
    if (connection)
        connection->setInputSuppressed(true);
    session->setVideoPaused(true);
    session->CleanUpHaptic();
    brls::Application::forceUnblockInputs();

    auto* menu = new StreamMenu();
    menu->setSleepAvailable(!host->isCloud());
    menu->setPlayersAvailable(!host->isCloud());

    menu->setConsoleName(host->getHostName());
    menu->setConsoleIsPs5(host->isPS5());
    menu->setStatsMode(session->getStatsOverlayMode());

    auto weak = weak_from_this();

    menu->setOnStatsToggle([weak](StatsOverlayMode mode) {
        if (auto self = weak.lock()) {
            brls::Logger::info("Stats overlay mode: {}", (int)mode);
            self->session->setStatsOverlayMode(mode);
        }
    });

    menu->setOnGyroReset([weak]() {
        if (auto self = weak.lock()) {
            brls::Logger::info("Gyro reset triggered from menu");
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

    menu->setOnPlayers([weak]() {
        auto self = weak.lock();
        if (!self)
            return;
        auto* input = self->session ? self->session->getInputManager() : nullptr;
        if (input == nullptr) {
            brls::Logger::warning("StreamMenu: no input manager for players panel");
            return;
        }
        auto* panel = new PlayersPanelView(self->host, self->connection, input);
        brls::Application::pushActivity(new brls::Activity(panel), brls::TransitionAnimation::NONE);
    });

    menu->setOnDisconnect([weak](bool sleep) {
        if (auto self = weak.lock()) {
            brls::Logger::info("Disconnect requested, sleep={}", sleep);
            self->disconnectWithSleep(sleep);
        }
    });

    menu->setOnDismiss([weak]() {
        if (auto self = weak.lock()) {
            brls::Logger::info("Menu dismissed");
            self->session->setVideoPaused(false);
            self->menuOpen = false;
            if (self->connection)
                self->connection->setInputSuppressed(false);
            brls::Application::blockInputs(true);
        }
    });

    brls::Application::pushActivity(new brls::Activity(menu));
    brls::Logger::info("showDisconnectMenu: menu opened");
}

void StreamView::abandonBeforeStart()
{
    brls::Logger::info("Pad choice abandoned before the stream started");

    intentionalDisconnect = true;
    padChoiceDone         = false;
    controllerReady       = false;

    if (session)
        session->FreeController();

    brls::Application::forceUnblockInputs();

    SharedViewHolder::release(this);

    akira::views::stream_nav::unwindToBase();
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
    SharedViewHolder::release(this);

    brls::sync([]() { akira::views::stream_nav::unwindToBase(); });
}

void StreamView::renderLogs(NVGcontext* vg, float x, float y, float width, float height)
{
    std::lock_guard<std::mutex> lock(logMutex);

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

    if (intentionalDisconnect || !settings->getAutoReconnect() || !sessionStarted || host->isCloud())
        return;

    const auto state = connection ? connection->state() : std::nullopt;
    const bool sessionAlive = state && (*state == libchiaking::play::SessionState::playing ||
                                        *state == libchiaking::play::SessionState::connecting);
    brls::Logger::info("StreamView::onFocusChanged(InFocus): streamActive={}, sessionAlive={}",
                       streamActive, sessionAlive);

    if (!streamActive || !sessionAlive) {
        brls::Logger::info("Stream dead on focus regain, prompting user");
        stopStream();
        brls::Application::forceUnblockInputs();

        auto weak = weak_from_this();
        auto* dialog = new brls::Dialog("akira/stream/disconnected_reconnect"_i18n);
        dialog->addButton("akira/stream/no"_i18n, [weak]() {
            if (auto self = weak.lock()) {
                SharedViewHolder::release(self.get());
            }
            akira::views::stream_nav::unwindToBase();
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
        brls::Logger::info("Already reconnecting, skipping");
        return;
    }

    reconnecting = true;

    brls::Logger::info("Auto-reconnect: tearing down old session");
    stopStream();

    currentStage = static_cast<int>(ConnectionStage::Idle);

    handover = libchiaking::play::ConsoleHandover::just_left;
    brls::Application::notify("akira/stream/reconnecting"_i18n);
    startStream();

    reconnecting = false;
}
