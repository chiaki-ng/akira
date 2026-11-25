#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

#include "trophies/icon_cache.hpp"

namespace akira::app {

class ArtCache {
public:
    using Bytes = trophies::IconCache::Bytes;

    enum class Shelf { art, trophies };

    static constexpr const char* artDirectory = "sdmc:/switch/akira/cache/art";
    static constexpr const char* trophyDirectory = "sdmc:/switch/akira/cache/trophies/icons";

    ArtCache(std::filesystem::path art = artDirectory, std::filesystem::path trophies = trophyDirectory);

    ArtCache(const ArtCache&) = delete;
    ArtCache& operator=(const ArtCache&) = delete;

    auto find(const std::string& url) -> std::optional<Bytes>;
    void keep(Shelf shelf, const std::string& url, const Bytes& bytes);
    void discard(const std::string& url);
    void clear(Shelf shelf);
    static auto itemLimit(Shelf shelf) -> std::size_t;

private:
    static constexpr std::size_t artMemory = 4 * 1024 * 1024;
    static constexpr std::size_t artItem = 4 * 1024 * 1024;
    static constexpr std::size_t trophyMemory = 12 * 1024 * 1024;
    static constexpr std::size_t trophyItem = 2 * 1024 * 1024;

    auto on(Shelf shelf) -> trophies::IconCache&;

    trophies::IconCache artShelf;
    trophies::IconCache trophyShelf;
};

}
