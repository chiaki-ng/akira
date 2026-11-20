#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <arisa/composition/exclusive_frame.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/input_block.hpp>
#include <arisa/runtime/input_passthrough.hpp>
#include <arisa/runtime/overlay_handle.hpp>

#include "app/services.hpp"
#include "app/ui_window.hpp"
#include "play/connect_flow.hpp"
#include "play/connection.hpp"
#include "play/menu_hold.hpp"
#include "play/stream_controls.hpp"
#include "play/ui/stream_commands.hpp"

namespace akira::consoles {
class Host;
}

namespace akira::app {
class Notifier;
}

namespace akira::app::arisa {

class ShellModel;
class Window;

class StreamDriver final : public play::ConnectionListener,
                           public play::ui::StreamCommands,
                           public std::enable_shared_from_this<StreamDriver> {
public:
    StreamDriver(Services& services, ::arisa::runtime::Application& application, Window& window, ShellModel& model,
        Notifier& notifier, std::function<void()> ended);
    ~StreamDriver() override;

    StreamDriver(const StreamDriver&) = delete;
    StreamDriver& operator=(const StreamDriver&) = delete;

    void watchFocus();
    void start(consoles::Host& host);
    void tick();
    auto active() const -> bool { return connection != nullptr; }

    void cancel() override;
    void reconnect() override;
    void resume() override;
    void leave(bool rest) override;
    void resetMotion() override;
    void showStats(play::StatsOverlayMode mode) override;
    auto stats() -> play::StreamStats override;

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
    void open(consoles::Host& host, libchiaking::play::ConsoleHandover handover);
    void enterVideo();
    void followMenu();
    void openDock();
    void closeDock();
    void focusChanged(bool focused);
    void stop();
    void fail(const std::string& text, bool reconnect);

    Services& services;
    ::arisa::runtime::Application& application;
    Window& window;
    ShellModel& model;
    Notifier& notifier;
    std::function<void()> ended;
    play::StreamControls controls;
    play::MenuHold menuHold;
    play::ConnectFlow flow;
    std::shared_ptr<play::Connection> connection;
    consoles::Host* host = nullptr;
    std::string lastConsole;
    bool videoActive = false;
    bool sessionStarted = false;
    bool intentionalDisconnect = false;
    std::unique_ptr<UiWindow::FocusWatch> focusWatch;
    std::optional<::arisa::runtime::InputBlock> padsOwned;
    std::optional<::arisa::runtime::InputPassthrough> dockButtons;
    std::optional<::arisa::composition::ExclusiveFrame> videoOwnsScreen;
    std::optional<::arisa::runtime::OverlayHandle> dock;
};

}
