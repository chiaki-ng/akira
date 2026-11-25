#pragma once

#include <cstddef>
#include <optional>

namespace akira::cloud {

class CatalogRows {
public:
    CatalogRows(std::size_t games, std::size_t perRow);

    auto rows() const -> std::size_t;
    auto gameAt(std::size_t row, std::size_t column) const -> std::optional<std::size_t>;
    auto perRow() const -> std::size_t { return columns; }

private:
    std::size_t games;
    std::size_t columns;
};

}
