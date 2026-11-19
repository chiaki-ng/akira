#pragma once

#include <cstdint>

#include <libchiaking/play_types.hpp>

#include "play/connection_stage.hpp"

namespace akira::play {

class ConnectFailure {
public:
    static const char* forStage(ConnectionStage stage);
    static const char* forOutcome(const libchiaking::play::Outcome& outcome);
    static const char* forRefusal(libchiaking::play::StartRefusal::Kind kind);
    static const char* quitReason(libchiaking::play::ConsoleQuit::Reason reason);
    static std::uint8_t seatStatus(const libchiaking::play::SeatRefusal& refusal);
};

}
