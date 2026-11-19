#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "cloud/title_refresh.hpp"

namespace akira::cloud {

class TitleRefreshCooldown {
public:
    using Clock = std::chrono::steady_clock;

    enum class Phase { Idle, Checking, CoolingDown };
    enum class Tick { Unchanged, Idle, Second };

    explicit TitleRefreshCooldown(Clock::duration cooldown);

    auto begin(Clock::time_point now) -> std::optional<uint64_t>;
    bool finish(uint64_t attempt, const TitleRefresh& refreshed, Clock::time_point now);
    auto tick(Clock::time_point now) -> Tick;

    auto phase() const -> Phase { return current; }
    bool idle() const { return current == Phase::Idle; }
    auto secondsLeft(Clock::time_point now) const -> int;
    auto outcome() const -> const std::optional<TitleRefresh::Outcome>& { return lastOutcome; }
    auto message() const -> const std::string& { return lastMessage; }

private:
    Clock::duration cooldown;
    Phase current = Phase::Idle;
    Clock::time_point readyAt{};
    int shownSecond = -1;
    uint64_t attempt = 0;
    std::optional<TitleRefresh::Outcome> lastOutcome;
    std::string lastMessage;
};

}
