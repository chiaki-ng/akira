#ifndef AKIRA_STREAM_VIEW_HPP
#define AKIRA_STREAM_VIEW_HPP

#include "log/log.hpp"
#include "log/connection_log.hpp"

#include <optional>

#include <vector>
#include <cstdint>
#include <borealis.hpp>
#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>

#if AKIRA_CLOUD
#include "cloud/views/launch_screen.hpp"
#endif
#include "app/background_work.hpp"
#include "consoles/discovery_manager.hpp"
#include "consoles/host.hpp"
#include "play/rumble_profile.hpp"
#include "play/connection.hpp"
#include "platform/stream/provider.hpp"
#include "play/session.hpp"
#include "settings/settings_manager.hpp"

namespace akira::play::views {

class StreamView : public brls::Box,
                   public ConnectionListener,
                   public akira::platform::StreamControlsHost,
                   public std::enable_shared_from_this<StreamView> {
public:
    explicit StreamView(consoles::Host* host, std::shared_ptr<consoles::Host> hostOwner = {});
    ~StreamView() override;

    void adoptConnectionLog(ConnectionLog log);

    // Must be called after make_shared to enable weak_from_this()
    void setupCallbacks();

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    brls::View* getDefaultFocus() override { return this; }

    void startStream();
    void stopStream();
    void setSessionAlreadyStarted(bool started) { sessionPreStarted = started; }

    static brls::View* create();

private:
    Session* controlsSession() override { return session; }
    std::shared_ptr<Connection> controlsConnection() override { return connection; }
    consoles::Host* controlsHost() override { return host; }
    bool controlsMenuOpen() const override { return menuOpen; }
    bool prepareController() override;
    void continueStart() override { startStream(); }
    void abandonStart() override;
    void pauseForUi() override { pauseStreamForUi(); }
    void resumeAfterUi() override { resumeStreamAfterUi(); }

    std::unique_ptr<akira::platform::StreamControlsProvider> controls;
    consoles::Host* host = nullptr;
    std::shared_ptr<consoles::Host> hostOwner;
    app::BackgroundWork::Pause backgroundPause;
    Session* session = nullptr;
    std::shared_ptr<Connection> connection;
    std::optional<ConnectionLog> connectionLog;
    libchiaking::play::ConsoleHandover handover = libchiaking::play::ConsoleHandover::fresh;
    SettingsManager* settings = nullptr;
    bool streamActive = false;
    bool sessionStarted = false;
    bool sessionPreStarted = false;
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
    std::chrono::steady_clock::time_point minusHoldStart;
    bool minusWasHeld = false;
    brls::Event<>::Subscription exitSubscription;
    brls::Event<bool>::Subscription focusSubscription;

    std::deque<std::string> logLines;
    std::atomic<int> currentStage{0};
    std::mutex logMutex;
    std::optional<log::Log::Subscription> logSubscription;
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

    static RumbleProfile activePadProfile();
    void openConnection();
    void pauseStreamForUi();
    void resumeStreamAfterUi();

    void checkMenuTrigger();
    void prepareVideoPipelineTick();
    void activateVideoPipeline();
    void streamingTick();
    void showDisconnectMenu();
    void disconnectWithSleep(bool sleep);

#if AKIRA_CLOUD
    void drawCloudLaunch(NVGcontext* vg, float x, float y, float width, float height);
    void pollCloudCancel();
    void showCloudFailure(const std::string& message, bool retry);

    std::optional<cloud::views::LaunchScreen> cloudScreen;
    bool cloudCancelHeld = true;
#endif

    void onFocusChanged(bool focused);
    void attemptReconnect();
};

}

#endif // AKIRA_STREAM_VIEW_HPP
