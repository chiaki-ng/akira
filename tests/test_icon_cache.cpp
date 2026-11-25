#include "test_util.hpp"

#include "trophies/icon_cache.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

namespace {

using akira::trophies::IconCache;

class IconDirectory {
public:
    IconDirectory()
        : root(std::filesystem::temp_directory_path()
               / std::format("akira-icons-{}-{}", std::chrono::steady_clock::now().time_since_epoch().count(),
                             counter++))
    {
        std::filesystem::create_directories(root);
    }

    ~IconDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }

    IconDirectory(const IconDirectory&) = delete;
    IconDirectory& operator=(const IconDirectory&) = delete;

    std::filesystem::path icons() const { return root / "cache" / "trophies" / "icons"; }

private:
    static inline std::atomic<int> counter{0};
    std::filesystem::path root;
};

IconCache::Bytes bytes(std::size_t count, std::uint8_t value)
{
    return IconCache::Bytes(count, value);
}

}

TEST(icon_cache_evicts_the_oldest_icon_past_the_memory_limit)
{
    IconDirectory directory;
    IconCache cache(directory.icons(), 10, 64);
    cache.remember("a", bytes(4, 1));
    cache.remember("b", bytes(4, 2));
    cache.remember("c", bytes(4, 3));
    CHECK(!cache.remembered("a"));
    CHECK(cache.remembered("b") == bytes(4, 2));
    CHECK(cache.remembered("c") == bytes(4, 3));
    CHECK_EQ(cache.rememberedBytes(), std::size_t{8});
}

TEST(icon_cache_keeps_the_icon_it_just_stored_even_over_the_limit)
{
    IconDirectory directory;
    IconCache cache(directory.icons(), 10, 64);
    cache.remember("small", bytes(4, 1));
    cache.remember("large", bytes(16, 2));
    CHECK(!cache.remembered("small"));
    CHECK(cache.remembered("large") == bytes(16, 2));
}

TEST(icon_cache_does_not_count_an_icon_twice)
{
    IconDirectory directory;
    IconCache cache(directory.icons(), 100, 64);
    cache.remember("a", bytes(4, 1));
    cache.remember("a", bytes(4, 9));
    CHECK_EQ(cache.rememberedBytes(), std::size_t{4});
    CHECK(cache.remembered("a") == bytes(4, 1));
}

TEST(icon_cache_reads_back_what_an_earlier_session_saved)
{
    IconDirectory directory;
    {
        IconCache first(directory.icons(), 100, 64);
        first.save("https://example.test/icon.png", bytes(12, 7));
    }
    IconCache second(directory.icons(), 100, 64);
    CHECK(second.load("https://example.test/icon.png") == bytes(12, 7));
    CHECK(!second.load("https://example.test/other.png"));
}

TEST(icon_cache_names_files_by_url_inside_its_directory)
{
    IconDirectory directory;
    IconCache cache(directory.icons(), 100, 64);
    const auto path = cache.pathFor("https://example.test/icon.png");
    CHECK(path.parent_path() == directory.icons());
    CHECK_EQ(path.extension().string(), std::string(".img"));
    CHECK_EQ(path.stem().string().size(), std::size_t{16});
    CHECK(cache.pathFor("https://example.test/icon.png") == path);
    CHECK(cache.pathFor("https://example.test/other.png") != path);
}

TEST(icon_cache_ignores_disk_icons_over_the_size_cap)
{
    IconDirectory directory;
    IconCache writer(directory.icons(), 100, 64);
    writer.save("big", bytes(32, 1));
    IconCache reader(directory.icons(), 100, 16);
    CHECK(!reader.load("big"));
}

TEST(icon_cache_ignores_empty_disk_icons)
{
    IconDirectory directory;
    IconCache cache(directory.icons(), 100, 64);
    cache.save("seed", bytes(1, 1));
    std::ofstream(cache.pathFor("empty"), std::ios::trunc);
    CHECK(!cache.load("empty"));
}

TEST(icon_cache_discard_forgets_memory_and_disk)
{
    IconDirectory directory;
    IconCache cache(directory.icons(), 100, 64);
    cache.remember("a", bytes(4, 1));
    cache.save("a", bytes(4, 1));
    cache.discard("a");
    CHECK(!cache.remembered("a"));
    CHECK(!cache.load("a"));
    CHECK_EQ(cache.rememberedBytes(), std::size_t{0});
}

TEST(icon_cache_clear_removes_every_icon_and_recreates_the_directory_on_use)
{
    IconDirectory directory;
    IconCache cache(directory.icons(), 100, 64);
    cache.remember("a", bytes(4, 1));
    cache.save("a", bytes(4, 1));
    cache.clear();
    CHECK(!std::filesystem::exists(directory.icons()));
    CHECK(!cache.remembered("a"));
    cache.save("b", bytes(4, 2));
    CHECK(cache.load("b") == bytes(4, 2));
}

TEST(icon_cache_stops_disk_writes_when_the_directory_cannot_be_written)
{
    IconDirectory directory;
    std::filesystem::create_directories(directory.icons().parent_path());
    std::ofstream(directory.icons(), std::ios::trunc) << "not a directory";
    IconCache cache(directory.icons(), 100, 64);
    CHECK(cache.savesToDisk());
    cache.save("a", bytes(4, 1));
    CHECK(!cache.savesToDisk());
    cache.remember("a", bytes(4, 1));
    CHECK(cache.remembered("a") == bytes(4, 1));
}
