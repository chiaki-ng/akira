#include "app/arisa/couch_bridge.hpp"

#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/provider.hpp>

#include <algorithm>
#include <utility>

#include "accounts/account_settings.hpp"
#include "app/app_settings.hpp"
#include "app/ui_dispatcher.hpp"
#include "log/log.hpp"
#include "platform/pads/pad_art.hpp"
#include "play/connection.hpp"
#include "play/couch_seats.hpp"

namespace akira::app::arisa {

CouchBridge::CouchBridge(play::CouchSeats& seats, const accounts::AccountSettings& accounts, const AppSettings& app,
    UiDispatcher& dispatcher)
    : seats(seats)
    , accounts(accounts)
    , app(app)
    , dispatcher(dispatcher)
{
}

CouchBridge::~CouchBridge()
{
    *alive = false;
    seats.setOnCouchJoined(nullptr);
    seats.setOnPadArrived(nullptr);
    seats.setOnCouchDisconnected(nullptr);
}

void CouchBridge::follow(Events followed)
{
    events = std::move(followed);
    seats.setOnCouchJoined([this, guard = alive](HidNpadIdType npad, std::uint8_t slot) {
        dispatcher.post([this, guard, npad, slot] {
            if (*guard && events.joined)
                events.joined(idOf(npad), slot);
        });
    });
    seats.setOnPadArrived([this, guard = alive](HidNpadIdType npad) {
        dispatcher.post([this, guard, npad] {
            if (*guard && events.arrived)
                events.arrived(idOf(npad));
        });
    });
    seats.setOnCouchDisconnected([this, guard = alive](HidNpadIdType, std::uint8_t slot) {
        dispatcher.post([this, guard, slot] {
            if (!*guard)
                return;
            log::info("[COUCH] player {} disconnected their pad", slot + 1);
            removePlayer(slot);
        });
    });
}

void CouchBridge::attach(std::weak_ptr<play::Connection> connection, bool ps5)
{
    session = std::move(connection);
    console5 = ps5;
}

void CouchBridge::detach()
{
    seats.cancelCouchJoin();
    session.reset();
}

auto CouchBridge::connection() const -> std::shared_ptr<play::Connection>
{
    return session.lock();
}

auto CouchBridge::kindOf(const platform::PadDescription& pad) -> PadKind
{
    switch (pad.kind)
    {
        case platform::PadPathKind::JoyCon: return PadKind::JoyCon;
        case platform::PadPathKind::SwitchPro: return PadKind::SwitchPro;
        case platform::PadPathKind::McPsNative: return PadKind::PlayStation;
        case platform::PadPathKind::McGeneric: break;
    }
    return PadKind::Generic;
}

auto CouchBridge::batteries(const platform::PadDescription& pad) -> std::vector<int>
{
    if (!pad.caps.battery)
        return {};
    if (platform::PadArt::twoBatteries(pad))
    {
        HidPowerInfo left{};
        HidPowerInfo right{};
        hidGetNpadPowerInfoSplit(pad.npad, &left, &right);
        return { static_cast<int>(left.battery_level), static_cast<int>(right.battery_level) };
    }
    HidPowerInfo info{};
    hidGetNpadPowerInfoSingle(pad.npad, &info);
    return { static_cast<int>(info.battery_level) };
}

auto CouchBridge::describe(const platform::PadDescription& pad, const std::map<HidNpadIdType, std::uint8_t>& roster) const
    -> Pad
{
    Pad made{
        .id = idOf(pad.npad),
        .label = pad.label,
        .kind = kindOf(pad),
        .art = platform::PadArt::path(pad),
        .handheld = pad.npad == HidNpadIdType_Handheld,
        .player = static_cast<int>(pad.npad) + 1,
        .touchpad = pad.caps.touchpad,
        .triggers = pad.caps.analog_triggers,
        .motion = pad.caps.gyro,
        .batteries = batteries(pad),
        .vendor = pad.vendor_id,
        .product = pad.product_id,
        .claimable = seats.isClaimableNpad(pad.npad),
        .primary = seats.CouchPadIndexForController(pad.npad) == 0,
    };
    if (const auto seated = roster.find(pad.npad); seated != roster.end())
        made.seat = seated->second;
    return made;
}

auto CouchBridge::pads() const -> std::vector<Pad>
{
    const std::map<HidNpadIdType, std::uint8_t> roster = seats.couchRoster();
    std::vector<Pad> listed;
    for (const platform::PadDescription& pad : seats.describePads())
        listed.push_back(describe(pad, roster));
    return listed;
}

auto CouchBridge::padsSettled() const -> bool
{
    if (!seats.backendAvailable())
        return true;
    return std::ranges::any_of(seats.describePads(), [](const platform::PadDescription& pad) {
        return pad.kind == platform::PadPathKind::McPsNative || pad.kind == platform::PadPathKind::McGeneric;
    });
}

void CouchBridge::choosePrimary(play::PadId pad)
{
    log::info("[COUCH] primary pad is npad {}", static_cast<std::uint32_t>(pad));
    seats.selectNpad(npadOf(pad));
}

void CouchBridge::watch(::arisa::runtime::ButtonCapture taken)
{
    capture.emplace(std::move(taken));
}

void CouchBridge::unwatch()
{
    capture.reset();
}

auto CouchBridge::slotOf(play::PadId pad) -> std::optional<std::size_t>
{
    const HidNpadIdType npad = npadOf(pad);
    if (npad == HidNpadIdType_Handheld)
        return 0;
    const auto slot = static_cast<std::size_t>(npad) - static_cast<std::size_t>(HidNpadIdType_No1);
    if (slot >= ::arisa::provider::max_pads)
        return std::nullopt;
    return slot;
}

auto CouchBridge::picking(play::PadId pad) -> bool
{
    using ::arisa::provider::Button;
    const std::optional<std::size_t> slot = slotOf(pad);
    if (!capture || !slot || !capture->connected(*slot))
        return false;
    const ::arisa::provider::Buttons held = capture->pad(*slot).buttons;
    return (held.contains(Button::l) && held.contains(Button::r)) || (held.contains(Button::zl) && held.contains(Button::zr))
        || (held.contains(Button::sl) && held.contains(Button::sr));
}

auto CouchBridge::degradedOnce() -> bool
{
    return seats.consumeDegradedNotice();
}

auto CouchBridge::seat(std::uint8_t slot) const -> play::CouchSeat
{
    if (const auto live = connection())
        return live->couchSeat(slot);
    return {};
}

auto CouchBridge::profileChoices(std::uint8_t slot) const -> std::vector<play::CouchProfileChoice>
{
    if (const auto live = connection())
        return live->couchProfileChoices(slot);
    return {};
}

auto CouchBridge::accountLabel(std::uint8_t slot) const -> std::string
{
    std::string account;
    if (slot == 0)
    {
        if (const accounts::Profile* active = accounts.activeProfile(); active && !active->accountId.empty())
            account = active->label();
    }
    else if (const auto live = connection())
        account = live->couchSeat(slot).accountLabel;
    return account.empty() ? account : app.maskAccountName(account);
}

auto CouchBridge::maskedName(const std::string& name) const -> std::string
{
    return app.maskAccountName(name);
}

auto CouchBridge::slotFree() const -> bool
{
    const auto live = connection();
    return live && live->couchSlotAvailable();
}

void CouchBridge::beginClaim()
{
    seats.beginCouchJoin();
}

void CouchBridge::cancelClaim()
{
    seats.cancelCouchJoin();
}

void CouchBridge::tickClaim()
{
    seats.tickCouchClaim();
}

auto CouchBridge::claimProgress() const -> float
{
    return seats.couchClaimProgress();
}

void CouchBridge::resolveArrival()
{
    seats.resolvePadArrival();
}

auto CouchBridge::addPlayer(std::uint8_t slot, std::optional<std::int64_t> profile) -> bool
{
    const auto live = connection();
    return live && live->addPlayer(slot, profile);
}

void CouchBridge::removePlayer(std::uint8_t slot)
{
    if (const auto live = connection())
        live->removePlayer(slot);
}

void CouchBridge::release(play::PadId pad)
{
    const HidNpadIdType npad = npadOf(pad);
    const std::uint8_t slot = seats.CouchPadIndexForController(npad);
    if (slot != play::CouchSeats::kNoCouchSlot && slot != 0)
    {
        std::uint8_t address[6]{};
        if (seats.couchSlotOutputTarget(slot, address, nullptr, nullptr))
            seats.unregisterCouchOutput(address);
    }
    seats.couchLeave(npad);
}

void CouchBridge::sendPasscode(std::uint8_t slot, const std::string& passcode)
{
    if (const auto live = connection())
        live->sendSeatPasscode(slot, passcode);
}

}
