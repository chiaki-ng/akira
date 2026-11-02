#include "cloud/launch_checks.hpp"

#include <algorithm>

namespace cloud {

LaunchChecks::LaunchChecks(std::string productId, uint64_t attempt, const Game& game)
    : product(std::move(productId)), attemptId(attempt), row(rowFor(game))
{
}

auto LaunchChecks::rowFor(const Game& game) -> std::vector<Step>
{
    if (game.streamServiceType == "psnow")
        return {Step::Membership, Step::Release};
    return {Step::Premium, Step::YourCopy, Step::CloudGame, Step::Restrictions, Step::Release};
}

auto LaunchChecks::stepOf(Check check) -> std::optional<Step>
{
    switch (check)
    {
        case Check::LoadingLibrary: return std::nullopt;
        case Check::Membership: return Step::Membership;
        case Check::CloudStreaming: return Step::Premium;
        case Check::Ownership:
        case Check::Entitlement: return Step::YourCopy;
        case Check::CloudGame: return Step::CloudGame;
        case Check::Exclusions: return Step::Restrictions;
        case Check::Release: return Step::Release;
    }
    return std::nullopt;
}

auto LaunchChecks::indexOf(Step step) const -> std::optional<std::size_t>
{
    auto it = std::ranges::find(row, step);
    if (it == row.end())
        return std::nullopt;
    return static_cast<std::size_t>(it - row.begin());
}

void LaunchChecks::observe(Check check)
{
    const auto step = stepOf(check);
    if (!isRunning() || !step || !indexOf(*step))
        return;
    if (reached && *indexOf(*step) < *indexOf(*reached))
        return;
    reached = step;
    current = Phase::Checking;
}

void LaunchChecks::clear()
{
    if (isRunning())
        current = Phase::Cleared;
}

void LaunchChecks::refuse(std::string reason)
{
    if (!isRunning())
        return;
    if (current != Phase::Checking)
        reached.reset();
    current = Phase::Refused;
    refusal = std::move(reason);
}

void LaunchChecks::launched()
{
    if (current == Phase::Cleared)
        current = Phase::Launched;
}

auto LaunchChecks::mark(Step step) const -> Mark
{
    const auto index = indexOf(step);
    if (!index)
        return Mark::Unchecked;
    switch (current)
    {
        case Phase::Starting: return Mark::Unchecked;
        case Phase::Cleared:
        case Phase::Launched: return Mark::Passed;
        case Phase::Checking:
        case Phase::Refused:
        {
            if (!reached)
                return Mark::Unchecked;
            const std::size_t position = *indexOf(*reached);
            if (*index < position)
                return Mark::Passed;
            if (*index > position)
                return Mark::Unchecked;
            return current == Phase::Checking ? Mark::Checking : Mark::Refused;
        }
    }
    return Mark::Unchecked;
}

} // namespace cloud
