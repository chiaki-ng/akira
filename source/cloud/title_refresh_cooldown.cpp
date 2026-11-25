#include "cloud/title_refresh_cooldown.hpp"

namespace akira::cloud {

TitleRefreshCooldown::TitleRefreshCooldown(Clock::duration cooldown)
    : cooldown(cooldown)
{
}

auto TitleRefreshCooldown::begin(Clock::time_point now) -> std::optional<uint64_t>
{
    if (current != Phase::Idle)
        return std::nullopt;
    current = Phase::Checking;
    lastOutcome.reset();
    lastMessage.clear();
    readyAt = now + cooldown;
    shownSecond = -1;
    return ++attempt;
}

bool TitleRefreshCooldown::finish(uint64_t answered, const TitleRefresh& refreshed, Clock::time_point now)
{
    if (answered != attempt || current != Phase::Checking)
        return false;
    current = secondsLeft(now) > 0 ? Phase::CoolingDown : Phase::Idle;
    lastOutcome = refreshed.outcome;
    lastMessage = refreshed.message;
    return true;
}

auto TitleRefreshCooldown::tick(Clock::time_point now) -> Tick
{
    if (current != Phase::CoolingDown)
        return Tick::Unchanged;
    const int left = secondsLeft(now);
    if (left <= 0)
    {
        current = Phase::Idle;
        return Tick::Idle;
    }
    if (left == shownSecond)
        return Tick::Unchanged;
    shownSecond = left;
    return Tick::Second;
}

auto TitleRefreshCooldown::secondsLeft(Clock::time_point now) const -> int
{
    const auto left = readyAt - now;
    if (left <= Clock::duration::zero())
        return 0;
    return static_cast<int>(std::chrono::ceil<std::chrono::seconds>(left).count());
}

}
