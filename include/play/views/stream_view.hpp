#ifndef AKIRA_STREAM_VIEW_HPP
#define AKIRA_STREAM_VIEW_HPP

#include "log/log.hpp"
#include "log/connection_log.hpp"
#include "log/log_tail.hpp"

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
#include "play/connect_flow.hpp"
#include "play/menu_hold.hpp"
#include "play/paused_frame.hpp"
#include "play/rumble_profile.hpp"
#include "play/connection.hpp"
#include "play/views/couch_seat_controls.hpp"
#include "play/views/stream_controls_host.hpp"
#include "play/session.hpp"
#include "play/stream_launcher.hpp"
#include "play/stream_route.hpp"
#include "app/view_services.hpp"

namespace akira::play::views {

class StreamView : public brls::Box,
                   public ConnectionListener,
                   public StreamControlsHost,
                   public std::enable_shared_from_this<StreamView> {
public:
    explicit StreamView(consoles::Host* host, std::shared_ptr<consoles::Host> hostOwner = {});
    ~StreamView() override;

    void adoptConnectionLog(log::ConnectionLog log);

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

    std::unique_ptr<CouchSeatControls> controls;
    consoles::Host* host = nullptr;
    std::shared_ptr<consoles::Host> hostOwner;
    app::BackgroundWork::Pause backgroundPause;
    Session* session = nullptr;
    std::shared_ptr<Connection> connection;
    std::optional<log::ConnectionLog> connectionLog;
    libchiaking::play::ConsoleHandover handover = libchiaking::play::ConsoleHandover::fresh;
    app::Services* services = nullptr;
    bool streamActive = false;
    bool sessionStarted = false;
    bool sessionPreStarted = false;
    bool controllerReady = false;

    bool menuOpen = false;
    bool wasMenuOpen = false;
    int drawCount = 0;

    PausedFrame pausedFrame;
    int pausedFrameImage = -1;

    void capturePausedFrame();
    void releasePausedFrame(NVGcontext* vg);
    bool drawPausedFrame(NVGcontext* vg, float x, float y, float width, float height);
    bool videoPipelineActive = false;
    bool intentionalDisconnect = false;
    bool reconnecting = false;
    bool loginPinEnteredThisSession = false;
    bool couchPasscodeDialogOpen = false;
    MenuHold menuHold;
    brls::Event<>::Subscription exitSubscription;
    brls::Event<bool>::Subscription focusSubscription;

    static constexpr size_t MAX_LOG_LINES = 30;
    log::LogTail logTail{MAX_LOG_LINES, log::LogTail::Stamp::None};
    ConnectFlow flow;
    std::optional<log::Log::Subscription> logSubscription;
    void startLogCapture();
    void stopLogCapture();
    void renderLogs(NVGcontext* vg, float x, float y, float width, float height);
    void drawStageRing(NVGcontext* vg, float x, float y, float width, float height);
    StreamRoute route() const;

    void onConnectProgress(const libchiaking::play::ConnectProgress& progress) override;
    void onLaunchProgress(const libchiaking::play::LaunchProgress& progress) override;
    void onPlaying() override;
    void onEnded(const libchiaking::play::Outcome& outcome) override;
    void onRefused(const libchiaking::play::StartRefusal& refusal) override;
    void onLoginPinNeeded(bool previousWasWrong) override;
    void onSeatPasscodeNeeded(uint8_t slot, bool retry) override;
    void onSeatRefused(uint8_t slot, const libchiaking::play::SeatRefusal& refusal) override;
    void onSeatDropped(uint8_t slot) override;

    void openConnection();
    static std::string failureText(StreamLauncher::Failure failure);
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
