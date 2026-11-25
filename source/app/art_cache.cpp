#include "app/art_cache.hpp"

#include <array>
#include <utility>

namespace akira::app {

ArtCache::ArtCache(std::filesystem::path art, std::filesystem::path trophies)
    : artShelf(std::move(art), artMemory, artItem)
    , trophyShelf(std::move(trophies), trophyMemory, trophyItem)
{
}

auto ArtCache::on(Shelf shelf) -> trophies::IconCache&
{
    return shelf == Shelf::trophies ? trophyShelf : artShelf;
}

auto ArtCache::itemLimit(Shelf shelf) -> std::size_t
{
    return shelf == Shelf::trophies ? trophyItem : artItem;
}

auto ArtCache::find(const std::string& url) -> std::optional<Bytes>
{
    constexpr std::array shelves{ Shelf::trophies, Shelf::art };
    for (const Shelf shelf : shelves)
        if (auto remembered = on(shelf).remembered(url))
            return remembered;
    for (const Shelf shelf : shelves)
        if (auto stored = on(shelf).load(url))
        {
            on(shelf).remember(url, *stored);
            return stored;
        }
    return std::nullopt;
}

void ArtCache::keep(Shelf shelf, const std::string& url, const Bytes& bytes)
{
    on(shelf).remember(url, bytes);
    on(shelf).save(url, bytes);
}

void ArtCache::discard(const std::string& url)
{
    artShelf.discard(url);
    trophyShelf.discard(url);
}

void ArtCache::clear(Shelf shelf)
{
    on(shelf).clear();
}

}
