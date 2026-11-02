#ifndef AKIRA_STREAM_VIEW_HPP
#define AKIRA_STREAM_VIEW_HPP

#include "util/connection_log.hpp"

#include <optional>

#include <vector>
#include <cstdint>
#include <borealis.hpp>
#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>

#include "cloud/launch_screen.hpp"
#include "core/background_work.hpp"
#include "core/discovery_manager.hpp"
#include "core/host.hpp"
#include "input/rumble_profile.hpp"
#include "stream/connection.hpp"
#include "stream/session.hpp"
#include "core/settings_manager.hpp"

class StreamView : public brls::Box,
                   public akira::stream::ConnectionListener,
                   public std::enable_shared_from_this<StreamView> {
public:
    explicit StreamView(Host* host, std::shared_ptr<Host> hostOwner = {});
    ~StreamView() override;

    void adoptConnectionLog(akira::ConnectionLog log);

    // Must be called after make_shared to enable weak_from_this()
    void setupCallbacks();

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    brls::View* getDefaultFocus() override { return this; }

    bool beginPadChoice();
    void startStream();
    void stopStream();
    void setSessionAlreadyStarted(bool started) { sessionPreStarted = started; }

    static brls::View* create();

private:
    void waitForPads(int attempt);
    void abandonBeforeStart();
    void finishPadChoice();

    Host* host = nullptr;
    std::shared_ptr<Host> hostOwner;
    akira::BackgroundWork::Pause backgroundPause;
    Session* session = nullptr;
    std::shared_ptr<akira::stream::Connection> connection;
    std::optional<akira::ConnectionLog> connectionLog;
    libchiaking::play::ConsoleHandover handover = libchiaking::play::ConsoleHandover::fresh;
    SettingsManager* settings = nullptr;
    bool streamActive = false;
    bool sessionStarted = false;
    bool sessionPreStarted = false;
    bool padChoiceDone = false;
    bool controllerReady = false;

    bool menuOpen = false;

    std::vector<uint8_t> pausedFrameRGBA;
    int pausedFrameW = 0;
    int pausedFrameH = 0;
    int pausedFrameImage = -1;

    void capturePausedFrame();
    void releasePausedFrame(NVGcontext* vg);
    bool drawPausedFrame(NVGcontext* vg, float x, float y, float width, float height);
    bool videoPipelineActive = false;
    bool intentionalDisconnect = false;
    bool reconnecting = false;
    bool loginPinEnteredThisSession = false;
    bool couchPasscodeDialogOpen = false;
    bool couchArrivalPromptOpen = false;
    std::chrono::steady_clock::time_point minusHoldStart;
    bool minusWasHeld = false;
    brls::Event<>::Subscription exitSubscription;
    brls::Event<bool>::Subscription focusSubscription;

    std::deque<std::string> logLines;
    std::atomic<int> currentStage{0};
    std::mutex logMutex;
    brls::Event<brls::Logger::TimePoint, brls::LogLevel, std::string>::Subscription logSubscription;
    bool logSubscriptionActive = false;
    static constexpr size_t MAX_LOG_LINES = 30;
    void startLogCapture();
    void stopLogCapture();
    void renderLogs(NVGcontext* vg, float x, float y, float width, float height);

    void onConnectProgress(const libchiaking::play::ConnectProgress& progress) override;
    void onLaunchProgress(const libchiaking::play::LaunchProgress& progress) override;
    void onPlaying() override;
    void onEnded(const libchiaking::play::Outcome& outcome) override;
    void onRefused(const libchiaking::play::StartRefusal& refusal) override;
    void onLoginPinNeeded(bool previousWasWrong) override;
    void onSeatPasscodeNeeded(uint8_t slot, bool retry) override;
    void onSeatRefused(uint8_t slot, const libchiaking::play::SeatRefusal& refusal) override;
    void onSeatDropped(uint8_t slot) override;

    static akira::input::RumbleProfile activePadProfile();
    void openConnection();
    void pauseStreamForUi();
    void resumeStreamAfterUi();
    void onPadArrived(HidNpadIdType npad);
    void openCouchClaim();
    void openPrimaryControllerPicker();

    void checkMenuTrigger();
    void prepareVideoPipelineTick();
    void activateVideoPipeline();
    void streamingTick();
    void showDisconnectMenu();
    void disconnectWithSleep(bool sleep);

    void drawCloudLaunch(NVGcontext* vg, float x, float y, float width, float height);
    void pollCloudCancel();
    void showCloudFailure(const std::string& message, bool retry);

    std::optional<cloud::LaunchScreen> cloudScreen;
    bool cloudCancelHeld = true;

    void onFocusChanged(bool focused);
    void attemptReconnect();
};

#endif // AKIRA_STREAM_VIEW_HPP
