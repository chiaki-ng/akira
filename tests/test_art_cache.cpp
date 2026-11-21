#include "test_util.hpp"

#include "app/art_cache.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>

namespace {

using akira::app::ArtCache;

class CacheRoot {
public:
    CacheRoot()
        : root(std::filesystem::temp_directory_path()
              / std::format("akira-art-{}-{}", std::chrono::steady_clock::now().time_since_epoch().count(), counter++))
    {
        std::filesystem::create_directories(root);
    }

    ~CacheRoot()
    {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }

    CacheRoot(const CacheRoot&) = delete;
    CacheRoot& operator=(const CacheRoot&) = delete;

    auto art() const -> std::filesystem::path { return root / "cache" / "art"; }
    auto trophies() const -> std::filesystem::path { return root / "cache" / "trophies" / "icons"; }

private:
    static inline std::atomic<int> counter{ 0 };
    std::filesystem::path root;
};

const std::string trophyIcon = "https://image.api.playstation.com/trophy/np/NPWR00001_00/icon.png";
const std::string cover = "https://image.api.playstation.com/vulcan/cover.png";
const ArtCache::Bytes trophyBytes{ 1, 2, 3 };
const ArtCache::Bytes coverBytes{ 4, 5, 6, 7 };

}

TEST(art_cache_finds_a_trophy_icon_for_an_art_lookup)
{
    CacheRoot root;
    ArtCache cache(root.art(), root.trophies());
    cache.keep(ArtCache::Shelf::trophies, trophyIcon, trophyBytes);
    const auto found = cache.find(trophyIcon);
    REQUIRE(found.has_value());
    CHECK(*found == trophyBytes);
}

TEST(art_cache_reads_either_shelf_from_disk_after_a_restart)
{
    CacheRoot root;
    {
        ArtCache cache(root.art(), root.trophies());
        cache.keep(ArtCache::Shelf::trophies, trophyIcon, trophyBytes);
        cache.keep(ArtCache::Shelf::art, cover, coverBytes);
    }
    ArtCache reopened(root.art(), root.trophies());
    CHECK(reopened.find(trophyIcon) == std::optional(trophyBytes));
    CHECK(reopened.find(cover) == std::optional(coverBytes));
}

TEST(art_cache_trophy_flush_keeps_cloud_covers)
{
    CacheRoot root;
    ArtCache cache(root.art(), root.trophies());
    cache.keep(ArtCache::Shelf::trophies, trophyIcon, trophyBytes);
    cache.keep(ArtCache::Shelf::art, cover, coverBytes);
    cache.clear(ArtCache::Shelf::trophies);
    CHECK(!cache.find(trophyIcon).has_value());
    CHECK(cache.find(cover) == std::optional(coverBytes));
    CHECK(std::filesystem::exists(root.art()));
    ArtCache reopened(root.art(), root.trophies());
    CHECK(reopened.find(cover) == std::optional(coverBytes));
}

TEST(art_cache_discard_drops_the_url_from_both_shelves)
{
    CacheRoot root;
    ArtCache cache(root.art(), root.trophies());
    cache.keep(ArtCache::Shelf::trophies, trophyIcon, trophyBytes);
    cache.keep(ArtCache::Shelf::art, trophyIcon, trophyBytes);
    cache.discard(trophyIcon);
    CHECK(!cache.find(trophyIcon).has_value());
}

TEST(art_cache_limits_each_shelf_like_before)
{
    CHECK(ArtCache::itemLimit(ArtCache::Shelf::trophies) == 2u * 1024u * 1024u);
    CHECK(ArtCache::itemLimit(ArtCache::Shelf::art) == 4u * 1024u * 1024u);
}
