#include "cloud/catalog_rows.hpp"

namespace akira::cloud {

CatalogRows::CatalogRows(std::size_t games, std::size_t perRow)
    : games(games)
    , columns(perRow)
{
}

auto CatalogRows::rows() const -> std::size_t
{
    return (games + columns - 1) / columns;
}

auto CatalogRows::gameAt(std::size_t row, std::size_t column) const -> std::optional<std::size_t>
{
    const std::size_t index = row * columns + column;
    if (column >= columns || index >= games)
        return std::nullopt;
    return index;
}

}
