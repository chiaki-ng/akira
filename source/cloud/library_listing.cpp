#include "cloud/library_listing.hpp"

#include <algorithm>
#include <cctype>
#include <ranges>

namespace cloud {

void LibraryListing::setGames(const std::vector<Game>& listedGames)
{
    games.clear();
    games.reserve(listedGames.size());
    for (const Game& game : listedGames)
        if (!game.trialEnded)
            games.push_back(game);
}

void LibraryListing::setRecentlyPlayed(std::vector<RecentlyPlayed> played)
{
    recentlyPlayed = std::move(played);
    recency.clear();
    for (std::size_t index = 0; index < recentlyPlayed.size(); ++index)
        recency.try_emplace(recentlyPlayed[index].productId, index);
}

void LibraryListing::setNow(int64_t now)
{
    nowMs = now;
}

void LibraryListing::setQuery(std::string text)
{
    searchText = std::move(text);
}

void LibraryListing::setPlatform(PlatformFilter platform)
{
    platformFilter = platform;
}

void LibraryListing::pick(LibraryFilter filter)
{
    chosen = FilterChoice::Picked;
    current = filter;
}

void LibraryListing::settle(bool choosing)
{
    if (current == LibraryFilter::Recent && !hasRecentlyPlayed())
        current = LibraryFilter::All;
    else if (current == LibraryFilter::Preorders && !hasPreorders())
        current = LibraryFilter::All;
    else if (choosing && chosen == FilterChoice::Automatic && hasRecentlyPlayed() && current != LibraryFilter::Recent)
        current = LibraryFilter::Recent;
}

auto LibraryListing::filters() const -> std::vector<LibraryFilter>
{
    std::vector<LibraryFilter> shown;
    for (LibraryFilter filter : allFilters)
    {
        if (filter == LibraryFilter::Recent && !hasRecentlyPlayed())
            continue;
        if (filter == LibraryFilter::Preorders && !hasPreorders())
            continue;
        shown.push_back(filter);
    }
    return shown;
}

bool LibraryListing::hasRecentlyPlayed() const
{
    return !recency.empty() && std::ranges::any_of(games, [this](const Game& game) { return isRecent(game); });
}

bool LibraryListing::hasPreorders() const
{
    return std::ranges::any_of(games, [this](const Game& game) { return game.awaitsRelease(nowMs); });
}

bool LibraryListing::isRecent(const Game& game) const
{
    return !game.isStoreOnly() && !game.refused && recency.contains(game.productId) && !game.awaitsRelease(nowMs);
}

bool LibraryListing::passes(const Game& game) const
{
    if (current == LibraryFilter::Preorders)
        return game.awaitsRelease(nowMs);
    if (game.awaitsRelease(nowMs))
        return false;
    switch (current)
    {
        case LibraryFilter::All: return !game.isStoreOnly() && !game.refused;
        case LibraryFilter::Recent: return isRecent(game);
        case LibraryFilter::Owned: return game.isOwnedPS5();
        case LibraryFilter::Plus: return game.isInCatalog();
        case LibraryFilter::Browse: return game.isStoreOnly();
        case LibraryFilter::Preorders: return true;
    }
    return false;
}

auto LibraryListing::matching() const -> std::vector<Game>
{
    const bool searching = !searchText.empty();
    std::vector<Game> matched;
    for (const Game& game : games)
    {
        if (!admits(platformFilter, game.platform))
            continue;
        if (searching ? nameContains(game.name, searchText) : passes(game))
            matched.push_back(game);
    }
    if (current == LibraryFilter::Recent && !searching)
    {
        std::ranges::stable_sort(matched, {}, [this](const Game& game) {
            auto it = recency.find(game.productId);
            return it == recency.end() ? recentlyPlayed.size() : it->second;
        });
    }
    return matched;
}

auto LibraryListing::find(std::string_view productId) const -> const Game*
{
    auto it = std::ranges::find(games, productId, &Game::productId);
    return it == games.end() ? nullptr : &*it;
}

auto LibraryListing::nextUnlock() const -> std::optional<int64_t>
{
    std::optional<int64_t> next;
    for (const Game& game : games)
        if (auto unlock = game.unlockAfter(nowMs); unlock && (!next || *unlock < *next))
            next = unlock;
    return next;
}

auto LibraryListing::lastPlayed(const Game& game) const -> std::optional<int64_t>
{
    auto it = recency.find(game.productId);
    if (it == recency.end())
        return std::nullopt;
    return recentlyPlayed[it->second].lastPlayedMs;
}

auto LibraryListing::key(LibraryFilter filter) -> std::string_view
{
    switch (filter)
    {
        case LibraryFilter::Recent: return "recent";
        case LibraryFilter::All: return "all";
        case LibraryFilter::Owned: return "owned";
        case LibraryFilter::Plus: return "plus";
        case LibraryFilter::Browse: return "browse";
        case LibraryFilter::Preorders: return "preorders";
    }
    return "all";
}

auto LibraryListing::filterNamed(std::string_view name) -> std::optional<LibraryFilter>
{
    for (LibraryFilter filter : allFilters)
        if (key(filter) == name)
            return filter;
    return std::nullopt;
}

bool LibraryListing::admits(PlatformFilter platform, std::string_view gamePlatform)
{
    switch (platform)
    {
        case PlatformFilter::All: return true;
        case PlatformFilter::PS5: return gamePlatform == "ps5";
        case PlatformFilter::PS4: return gamePlatform == "ps4";
        case PlatformFilter::PS3: return gamePlatform == "ps3";
    }
    return true;
}

bool LibraryListing::nameContains(std::string_view name, std::string_view text)
{
    const auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
    return !std::ranges::search(name, text, {}, lower, lower).empty();
}

} // namespace cloud
