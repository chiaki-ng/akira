#ifndef AKIRA_CLOUD_LIBRARY_LISTING_HPP
#define AKIRA_CLOUD_LIBRARY_LISTING_HPP

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cloud/models.hpp"

namespace cloud {

enum class LibraryFilter {
    Recent,
    All,
    Owned,
    Plus,
    Browse,
    Preorders
};

enum class PlatformFilter {
    All,
    PS5,
    PS4,
    PS3
};

enum class FilterChoice {
    Automatic,
    Picked
};

struct RecentlyPlayed {
    std::string productId;
    int64_t lastPlayedMs = 0;
};

class LibraryListing {
public:
    static constexpr LibraryFilter allFilters[] = {
        LibraryFilter::Recent, LibraryFilter::All, LibraryFilter::Owned,
        LibraryFilter::Plus, LibraryFilter::Browse, LibraryFilter::Preorders,
    };
    static constexpr PlatformFilter allPlatforms[] = {
        PlatformFilter::All, PlatformFilter::PS5, PlatformFilter::PS4, PlatformFilter::PS3,
    };

    void setGames(const std::vector<Game>& games);
    void setRecentlyPlayed(std::vector<RecentlyPlayed> played);
    void setNow(int64_t nowMs);
    void setQuery(std::string text);
    void setPlatform(PlatformFilter platform);
    void pick(LibraryFilter filter);
    void settle(bool choosing);

    auto filters() const -> std::vector<LibraryFilter>;
    auto matching() const -> std::vector<Game>;
    auto listed() const -> const std::vector<Game>& { return games; }
    auto find(std::string_view productId) const -> const Game*;
    auto nextUnlock() const -> std::optional<int64_t>;
    auto lastPlayed(const Game& game) const -> std::optional<int64_t>;

    auto filter() const -> LibraryFilter { return current; }
    auto choice() const -> FilterChoice { return chosen; }
    auto platform() const -> PlatformFilter { return platformFilter; }
    auto query() const -> const std::string& { return searchText; }
    auto now() const -> int64_t { return nowMs; }
    bool hasRecentlyPlayed() const;
    bool hasPreorders() const;

    static auto key(LibraryFilter filter) -> std::string_view;
    static auto filterNamed(std::string_view key) -> std::optional<LibraryFilter>;
    static bool admits(PlatformFilter platform, std::string_view gamePlatform);

private:
    bool isRecent(const Game& game) const;
    bool passes(const Game& game) const;
    static bool nameContains(std::string_view name, std::string_view text);

    std::vector<Game> games;
    std::vector<RecentlyPlayed> recentlyPlayed;
    std::map<std::string, std::size_t, std::less<>> recency;
    int64_t nowMs = 0;
    std::string searchText;
    PlatformFilter platformFilter = PlatformFilter::All;
    LibraryFilter current = LibraryFilter::All;
    FilterChoice chosen = FilterChoice::Automatic;
};

} // namespace cloud

#endif // AKIRA_CLOUD_LIBRARY_LISTING_HPP
