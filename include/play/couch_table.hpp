#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "play/couch_seat.hpp"

namespace akira::play {

enum class PadId : std::uint32_t {};

class CouchTable {
public:
    enum class PadKind { JoyCon, SwitchPro, PlayStation, Generic };

    struct Pad {
        PadId id{};
        std::string label;
        PadKind kind = PadKind::JoyCon;
        std::string art;
        bool handheld = false;
        int player = 0;
        bool touchpad = false;
        bool triggers = false;
        bool motion = false;
        std::vector<int> batteries;
        std::uint16_t vendor = 0;
        std::uint16_t product = 0;
        bool claimable = false;
        bool primary = false;
        std::optional<std::uint8_t> seat;

        friend bool operator==(const Pad&, const Pad&) = default;
    };

    static constexpr std::uint8_t seatCount = 4;

    virtual ~CouchTable() = default;

    virtual auto pads() const -> std::vector<Pad> = 0;
    virtual auto padsSettled() const -> bool = 0;
    virtual void choosePrimary(PadId pad) = 0;
    virtual auto picking(PadId pad) -> bool = 0;
    virtual auto degradedOnce() -> bool = 0;

    virtual auto ps5() const -> bool = 0;
    virtual auto seat(std::uint8_t slot) const -> CouchSeat = 0;
    virtual auto profileChoices(std::uint8_t slot) const -> std::vector<CouchProfileChoice> = 0;
    virtual auto accountLabel(std::uint8_t slot) const -> std::string = 0;
    virtual auto maskedName(const std::string& name) const -> std::string = 0;
    virtual auto slotFree() const -> bool = 0;

    virtual void beginClaim() = 0;
    virtual void cancelClaim() = 0;
    virtual void tickClaim() = 0;
    virtual auto claimProgress() const -> float = 0;
    virtual void resolveArrival() = 0;

    [[nodiscard]] virtual auto addPlayer(std::uint8_t slot, std::optional<std::int64_t> profile) -> bool = 0;
    virtual void removePlayer(std::uint8_t slot) = 0;
    virtual void release(PadId pad) = 0;
    virtual void sendPasscode(std::uint8_t slot, const std::string& passcode) = 0;
};

}
