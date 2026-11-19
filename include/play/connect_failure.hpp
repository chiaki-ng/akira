#pragma once

#include <cstdint>

#include <libchiaking/detail/play/outcome.hpp>
#include <libchiaking/detail/play/seats.hpp>
#include <libchiaking/detail/play/start_refusal.hpp>

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
