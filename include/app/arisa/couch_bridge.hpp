#pragma once

#include <switch.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>

#include <arisa/runtime/button_capture.hpp>

#include "play/couch_table.hpp"
#include "platform/pads/pad.hpp"

namespace akira::accounts {
class AccountSettings;
}

namespace akira::app {
class AppSettings;
class UiDispatcher;
}

namespace akira::play {
class Connection;
class CouchSeats;
}

namespace akira::app::arisa {

class CouchBridge final : public play::CouchTable {
public:
    struct Events {
        std::function<void(play::PadId, std::uint8_t)> joined;
        std::function<void(play::PadId)> arrived;
    };

    CouchBridge(play::CouchSeats& seats, const accounts::AccountSettings& accounts, const AppSettings& app,
        UiDispatcher& dispatcher);
    ~CouchBridge() override;

    CouchBridge(const CouchBridge&) = delete;
    CouchBridge& operator=(const CouchBridge&) = delete;

    void follow(Events events);
    void attach(std::weak_ptr<play::Connection> connection, bool ps5);
    void detach();
    void watch(::arisa::runtime::ButtonCapture capture);
    void unwatch();

    auto pads() const -> std::vector<Pad> override;
    auto padsSettled() const -> bool override;
    void choosePrimary(play::PadId pad) override;
    auto picking(play::PadId pad) -> bool override;
    auto degradedOnce() -> bool override;

    auto ps5() const -> bool override { return console5; }
    auto seat(std::uint8_t slot) const -> play::CouchSeat override;
    auto profileChoices(std::uint8_t slot) const -> std::vector<play::CouchProfileChoice> override;
    auto accountLabel(std::uint8_t slot) const -> std::string override;
    auto maskedName(const std::string& name) const -> std::string override;
    auto slotFree() const -> bool override;

    void beginClaim() override;
    void cancelClaim() override;
    void tickClaim() override;
    auto claimProgress() const -> float override;
    void resolveArrival() override;

    [[nodiscard]] auto addPlayer(std::uint8_t slot, std::optional<std::int64_t> profile) -> bool override;
    void removePlayer(std::uint8_t slot) override;
    void release(play::PadId pad) override;
    void sendPasscode(std::uint8_t slot, const std::string& passcode) override;

private:
    static auto npadOf(play::PadId pad) -> HidNpadIdType { return static_cast<HidNpadIdType>(pad); }
    static auto idOf(HidNpadIdType npad) -> play::PadId { return static_cast<play::PadId>(npad); }
    static auto slotOf(play::PadId pad) -> std::optional<std::size_t>;
    static auto kindOf(const platform::PadDescription& pad) -> PadKind;
    static auto batteries(const platform::PadDescription& pad) -> std::vector<int>;
    auto describe(const platform::PadDescription& pad, const std::map<HidNpadIdType, std::uint8_t>& roster) const -> Pad;
    auto connection() const -> std::shared_ptr<play::Connection>;

    play::CouchSeats& seats;
    const accounts::AccountSettings& accounts;
    const AppSettings& app;
    UiDispatcher& dispatcher;
    std::weak_ptr<play::Connection> session;
    bool console5 = false;
    Events events;
    std::optional<::arisa::runtime::ButtonCapture> capture;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

}
