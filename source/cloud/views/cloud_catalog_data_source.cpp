#include "cloud/views/cloud_catalog_data_source.hpp"

#include <utility>

#include "cloud/catalog_rows.hpp"
#include "cloud/views/cloud_game_row_cell.hpp"

namespace akira::cloud::views {

CloudCatalogDataSource::CloudCatalogDataSource(std::vector<Game> games, std::function<void(const Game&)> onLaunch,
    std::function<void(const Game&)> onPin)
    : games(std::move(games)), onLaunch(std::move(onLaunch)), onPin(std::move(onPin))
{
}

size_t CloudCatalogDataSource::getItemCount()
{
    return CatalogRows(games.size(), CloudGameRowCell::PerRow).rows();
}

float CloudCatalogDataSource::heightForRow(brls::View* recycler, size_t index)
{
    return CloudGameRowCell::Height;
}

app::views::RecyclingGridItem* CloudCatalogDataSource::cellForRow(app::views::RecyclingView* recycler, size_t index)
{
    auto* cell = dynamic_cast<CloudGameRowCell*>(recycler->dequeueReusableCell("CloudRow"));
    if (!cell)
        return nullptr;
    cell->bindRow(games, index, onLaunch, onPin);
    return cell;
}

void CloudCatalogDataSource::clearData()
{
    games.clear();
}

}
