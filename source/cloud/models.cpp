#include "cloud/models.hpp"

#include <format>

namespace cloud {

bool Game::awaitsRelease(int64_t nowMs) const
{
    switch (release)
    {
        case ReleaseKind::Unrestricted: return false;
        case ReleaseKind::Unannounced: return true;
        case ReleaseKind::Scheduled: return availableFromMs > nowMs;
    }
    return false;
}

std::optional<int64_t> Game::unlockAfter(int64_t nowMs) const
{
    if (release == ReleaseKind::Scheduled && availableFromMs > nowMs)
        return availableFromMs;
    return std::nullopt;
}

std::string Game::countdown(int64_t seconds)
{
    const int64_t total = seconds < 0 ? 0 : seconds;
    const std::string clock = std::format("{:02}:{:02}:{:02}", total % 86400 / 3600, total % 3600 / 60, total % 60);
    const int64_t days = total / 86400;
    return days == 0 ? clock : std::format("{}d {}", days, clock);
}

std::string Game::artworkUrl() const
{
    return !landscapeImageUrl.empty() ? landscapeImageUrl : imageUrl;
}

int Catalog::launchableCount() const
{
    int count = 0;
    for (const Game& game : games)
        count += game.launchable() ? 1 : 0;
    return count;
}

const Game* Catalog::find(const Game& game) const
{
    for (const Game& candidate : games)
        if (candidate.productId == game.productId && candidate.streamServiceType == game.streamServiceType)
            return &candidate;
    return nullptr;
}

std::string platformBadge(const Game& game)
{
    if (game.platform == "ps5")
        return "PS5";
    if (game.platform == "ps4")
        return "PS4";
    if (game.platform == "ps3")
        return "PS3";
    return "";
}

std::string categoryBadge(const Game& game)
{
    if (game.category == "owned")
        return "Owned";
    if (game.category == "streamable")
        return "Catalog";
    if (game.category == "purchaseable")
        return "Store";
    return "";
}

} // namespace cloud
