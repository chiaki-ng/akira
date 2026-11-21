#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <arisa/composition/element.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/input_block.hpp>
#include <arisa/runtime/input_passthrough.hpp>
#include <arisa/runtime/overlay_handle.hpp>

#include "app/arisa/stream_stage.hpp"
#include "app/services.hpp"
#include "app/ui_window.hpp"
#include "cloud/cloud_launch.hpp"
#include "cloud/launch_report.hpp"
#include "log/log.hpp"
#include "log/log_tail.hpp"
#include "play/connect_flow.hpp"
#include "play/connection.hpp"
#include "play/session_ending.hpp"
#include "play/stream_launcher.hpp"
#include "play/ui/connecting_screen.hpp"
#include "play/couch_table.hpp"
#include "play/menu_hold.hpp"
#include "play/stats_hud_placement.hpp"
#include "play/diagnostics_history.hpp"
#include "play/diagnostics_report.hpp"
#include "play/stream_controls.hpp"
#include "play/ui/couch.hpp"
#include "play/ui/mapping.hpp"
#include "play/ui/stats_board.hpp"
#include "play/ui/stream_commands.hpp"
#include "settings/ui/control_options.hpp"
#include "settings/ui/picture_options.hpp"
#include "settings/ui/sound_options.hpp"

namespace akira::consoles {
class Host;
}

namespace akira::app {
class Notifier;
}

namespace akira::app::arisa {

class CouchBridge;
class ShellModel;
class Window;

class StreamDriver final : public play::ConnectionListener,
                           public play::ui::StreamCommands,
                           public std::enable_shared_from_this<StreamDriver> {
public:
    struct Couch {
        CouchBridge* table = nullptr;
        play::ui::Couch* model = nullptr;
    };

    using CloudRetry = std::function<void(const cloud::CloudLaunch&)>;

    StreamDriver(Services& services, StreamStage& stage, Window& window, ShellModel& model, Notifier& notifier, settings::ui::ControlOptions& controlOptions, settings::ui::PictureOptions& picture,
        settings::ui::SoundOptions& sound, play::ui::Mapping& mapping, Couch couch, std::function<void()> ended,
        CloudRetry retryCloud);
    ~StreamDriver() override;

    StreamDriver(const StreamDriver&) = delete;
    StreamDriver& operator=(const StreamDriver&) = delete;

    void watchFocus();
    void start(consoles::Host& host);
    void startCloud(std::shared_ptr<consoles::Host> host);
    void tick();
    void padArrived(play::PadId pad);
    auto active() const -> bool { return connection != nullptr; }

    void cancel() override;
    void reconnect() override;
    void restart() override;
    void resume() override;
    void leave(bool rest) override;
    void goHome() override;
    void resetMotion() override;
    void tuneColour(play::ColourAdjustments colour) override;
    void tuneVolume(play::Volume volume) override;
    void showDiagnostics(play::DiagnosticsAppearance appearance) override;
    auto diagnostics() -> play::DiagnosticsReport override;
    void submitLoginPin(consoles::ConsolePin pin) override;
    void cancelLoginPin() override;

    void onConnectProgress(const libchiaking::play::ConnectProgress& progress) override;
    void onLaunchProgress(const libchiaking::play::LaunchProgress& progress) override;
    void onPlaying() override;
    void onEnded(const libchiaking::play::Outcome& outcome) override;
    void onRefused(const libchiaking::play::StartRefusal& refusal) override;
    void onLoginPinNeeded(bool previousWasWrong) override;
    void onSeatPasscodeNeeded(uint8_t slot, bool retry) override;
    void onSeatRefused(uint8_t slot, const libchiaking::play::SeatRefusal& refusal) override;
    void onSeatDropped(uint8_t slot) override;

private:
    using Clock = std::chrono::steady_clock;

    static constexpr std::chrono::milliseconds statsInterval{ 250 };
    static constexpr std::chrono::milliseconds historyInterval{ 500 };
    static constexpr std::size_t connectLogLines = 100;

    void begin(consoles::Host& host);
    void askAboutFileLogging(consoles::Host& host);
    void fileLoggingAnswered(bool turnOff);
    void askLoginPin(bool retry);
    void followConnectLog(const consoles::Host& host);
    void stopConnectLog();
    auto connectingProgress() const -> play::ui::ConnectingScreen::Progress;
    void open(consoles::Host& host, libchiaking::play::ConsoleHandover handover);
    [[nodiscard]] auto prepareController() -> bool;
    void waitForPads();
    void finishPadChoice();
    void padChosen(play::ui::Couch::Pick pick, std::optional<play::PadId> pad);
    void showPicker(play::ui::Couch::Picked picked);
    void closePicker();
    void arrivalAnswered(play::ui::Couch::Arrival answer);
    void passcodeClosed();
    void pauseForUi();
    void resumeAfterUi();
    auto couchVisible() const -> bool;
    auto modal(::arisa::composition::Element content) -> ::arisa::runtime::OverlayHandle;
    void enterVideo();
    void followStage();
    void followMenu();
    void openDock(std::optional<play::StreamControls::Control> opening = std::nullopt);
    void closeDock();
    void focusChanged(bool focused);
    void showHud();
    void hideHud();
    void followHud();
    void adoptHudPosition();
    void keepHudPosition();
    auto savedHudPosition() const -> std::optional<play::TouchClaim::Fraction>;
    auto decoder() -> std::string;
    void sampleDiagnostics();
    auto cloudBandwidth() -> std::optional<std::string>;
    static auto overlayMode(play::DiagnosticsAppearance::Level level) -> play::StatsOverlayMode;
    void stop();
    auto offersRest() const -> bool;
    void askRest();
    void finishLeave(bool rest);
    static auto failureKey(play::StreamLauncher::Failure failure) -> const char*;
    auto launchProgress() const -> play::ui::ConnectingScreen::Progress;
    auto cloudStopped(const play::SessionEnding& ending, const libchiaking::play::Outcome& outcome) const -> std::string;
    void fail(const std::string& text, bool reconnect, bool retry = false);

    Services& services;
    StreamStage& stage;
    Window& window;
    ShellModel& model;
    settings::ui::ControlOptions& controlOptions;
    settings::ui::PictureOptions& picture;
    settings::ui::SoundOptions& sound;
    play::ui::Mapping& mapping;
    Couch couch;
    Notifier& notifier;
    std::function<void()> ended;
    CloudRetry retryCloud;
    cloud::LaunchReport launchReport;
    std::optional<cloud::CloudLaunch> lastCloud;
    play::StatsHudPlacement hudPlacement;
    play::ui::StatsBoard statsBoard;
    Clock::time_point nextStatsSample{};
    std::optional<Clock::time_point> lastHistorySample;
    play::DiagnosticsHistory history;
    std::uint64_t hudRevision = 0;
    bool hudPositionUnsaved = false;
    play::StreamControls controls;
    play::MenuHold menuHold;
    play::ConnectFlow flow;
    std::shared_ptr<play::Connection> connection;
    consoles::Host* host = nullptr;
    std::shared_ptr<consoles::Host> owned;
    std::string lastConsole;
    bool videoActive = false;
    bool videoStarting = false;
    bool sessionStarted = false;
    bool intentionalDisconnect = false;
    bool controllerReady = false;
    bool waitingForPads = false;
    int padAttempt = 0;
    Clock::time_point nextPadPoll{};
    consoles::Host* pending = nullptr;
    consoles::Host* asking = nullptr;
    bool loginPinEntered = false;
    std::optional<log::LogTail> connectLog;
    std::optional<log::Log::Subscription> connectLogFollow;
    bool arrivalDock = false;
    bool pausedForPasscode = false;
    std::unique_ptr<UiWindow::FocusWatch> focusWatch;
    std::optional<::arisa::runtime::InputBlock> padsOwned;
    std::optional<::arisa::runtime::OverlayHandle> dock;
    std::optional<::arisa::runtime::InputPassthrough> dockInput;
    std::optional<::arisa::runtime::InputPassthrough> pickerInput;
    std::optional<::arisa::runtime::OverlayHandle> hud;
    std::optional<::arisa::runtime::OverlayHandle> confirmLeave;
    std::optional<::arisa::runtime::OverlayHandle> picker;
    std::optional<::arisa::runtime::OverlayHandle> prompt;
    std::optional<::arisa::runtime::OverlayHandle> passcode;
    std::optional<::arisa::runtime::OverlayHandle> fileLogQuestion;
    std::optional<::arisa::runtime::OverlayHandle> loginPin;
};

}
