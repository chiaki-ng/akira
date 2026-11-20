#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "play/couch_table.hpp"

namespace tests {

class FakeCouchTable final : public akira::play::CouchTable {
public:
    static auto pad(std::uint32_t id, std::string label, PadKind kind, bool triggers, bool touchpad) -> Pad
    {
        return Pad{ .id = static_cast<akira::play::PadId>(id),
            .label = std::move(label),
            .kind = kind,
            .art = kind == PadKind::PlayStation ? "img/pads/dualsense.png" : "img/pads/switch_pro.png",
            .player = static_cast<int>(id) + 1,
            .touchpad = touchpad,
            .triggers = triggers,
            .motion = true,
            .batteries = { 3 },
            .claimable = id != 0,
            .primary = id == 0 };
    }

    auto pads() const -> std::vector<Pad> override { return listed; }
    auto padsSettled() const -> bool override { return settled; }
    void choosePrimary(akira::play::PadId chosen) override { primary = chosen; }
    auto picking(akira::play::PadId held) -> bool override { return picked.contains(static_cast<std::uint32_t>(held)); }
    auto degradedOnce() -> bool override { return std::exchange(degraded, false); }

    auto ps5() const -> bool override { return console5; }
    auto seat(std::uint8_t slot) const -> akira::play::CouchSeat override
    {
        const auto found = seats.find(slot);
        return found == seats.end() ? akira::play::CouchSeat{} : found->second;
    }
    auto profileChoices(std::uint8_t) const -> std::vector<akira::play::CouchProfileChoice> override { return choices; }
    auto accountLabel(std::uint8_t slot) const -> std::string override
    {
        if (slot == 0)
            return "kyogre_404";
        return seat(slot).accountLabel;
    }
    auto maskedName(const std::string& name) const -> std::string override { return name; }
    auto slotFree() const -> bool override { return true; }

    void beginClaim() override { ++claims; }
    void cancelClaim() override { ++cancels; }
    void tickClaim() override { ++ticks; }
    auto claimProgress() const -> float override { return progress; }
    void resolveArrival() override { ++arrivals; }

    auto addPlayer(std::uint8_t slot, std::optional<std::int64_t> profile) -> bool override
    {
        added.emplace_back(slot, profile);
        if (!accept)
            return false;
        seats[slot] = akira::play::CouchSeat{ .phase = akira::play::SeatPhase::Joining, .profileId = profile.value_or(0) };
        return true;
    }
    void removePlayer(std::uint8_t slot) override
    {
        removed.push_back(slot);
        seats.erase(slot);
    }
    void release(akira::play::PadId gone) override { released.push_back(gone); }
    void sendPasscode(std::uint8_t slot, const std::string& passcode) override { passcodes.emplace_back(slot, passcode); }

    void seatPad(std::size_t index, std::uint8_t slot, akira::play::CouchSeat state)
    {
        listed[index].seat = slot;
        seats[slot] = std::move(state);
    }

    std::vector<Pad> listed;
    std::map<std::uint8_t, akira::play::CouchSeat> seats;
    std::vector<akira::play::CouchProfileChoice> choices;
    std::set<std::uint32_t> picked;
    std::optional<akira::play::PadId> primary;
    std::vector<std::pair<std::uint8_t, std::optional<std::int64_t>>> added;
    std::vector<std::uint8_t> removed;
    std::vector<akira::play::PadId> released;
    std::vector<std::pair<std::uint8_t, std::string>> passcodes;
    bool console5 = true;
    bool settled = true;
    bool degraded = false;
    bool accept = true;
    float progress = 0.0F;
    int claims = 0;
    int cancels = 0;
    int ticks = 0;
    int arrivals = 0;
};

}
