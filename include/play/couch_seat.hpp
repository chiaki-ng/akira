#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <libchiaking/play_types.hpp>

namespace akira::play {

enum class SeatPhase : uint8_t {
    Empty,
    Joining,
    Passcode,
    Playing,
    Failed
};

struct CouchSeat {
    SeatPhase phase = SeatPhase::Empty;
    std::optional<libchiaking::play::SeatRefusal> refusal;
    bool dropped = false;
    std::string accountLabel;
    int64_t profileId = 0;
};

struct CouchProfileChoice {
    int64_t profileId = 0;
    std::string label;
    std::string accountId;
    std::string avatarUrl;
};

}
