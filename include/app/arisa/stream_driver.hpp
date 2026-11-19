#pragma once

#include <memory>
#include <optional>
#include <string>

#include <arisa/composition/exclusive_frame.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/input_block.hpp>
#include <arisa/runtime/input_passthrough.hpp>
#include <arisa/runtime/overlay_handle.hpp>

#include "app/services.hpp"
#include "play/connect_flow.hpp"
#include "play/connection.hpp"
#include "play/menu_hold.hpp"
#include "play/stream_controls.hpp"

namespace akira::consoles {
class Host;
}

namespace akira::app::arisa {

class ShellModel;
class Window;

class StreamDriver final : public play::ConnectionListener, public std::enable_shared_from_this<StreamDriver> {
public:
    StreamDriver(Services& services, ::arisa::runtime::Application& application, Window& window, ShellModel& model);
    ~StreamDriver() override;

    StreamDriver(const StreamDriver&) = delete;
    StreamDriver& operator=(const StreamDriver&) = delete;

    void start(consoles::Host& host);
    void cancel();
    void tick();
    void resume();
    void leave(bool rest);
    auto active() const -> bool { return connection != nullptr; }

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
    void enterVideo();
    void followMenu();
    void openPanel();
    void stop(std::string status);

    Services& services;
    ::arisa::runtime::Application& application;
    Window& window;
    ShellModel& model;
    play::StreamControls controls;
    play::MenuHold menuHold;
    play::ConnectFlow flow;
    std::shared_ptr<play::Connection> connection;
    consoles::Host* host = nullptr;
    bool videoActive = false;
    std::optional<::arisa::runtime::InputBlock> padsOwned;
    std::optional<::arisa::runtime::InputPassthrough> panelButtons;
    std::optional<::arisa::composition::ExclusiveFrame> videoOwnsScreen;
    std::optional<::arisa::runtime::OverlayHandle> panel;
};

}
