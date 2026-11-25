#pragma once

#include <functional>
#include <vector>

#include "app/views/vendored/switchfin/recycling_grid.hpp"
#include "cloud/models.hpp"

namespace akira::cloud::views {

class CloudCatalogDataSource : public app::views::RecyclingGridDataSource {
public:
    CloudCatalogDataSource(std::vector<Game> games, std::function<void(const Game&)> onLaunch,
        std::function<void(const Game&)> onPin);

    size_t getItemCount() override;
    float heightForRow(brls::View* recycler, size_t index) override;
    app::views::RecyclingGridItem* cellForRow(app::views::RecyclingView* recycler, size_t index) override;
    void clearData() override;

private:
    std::vector<Game> games;
    std::function<void(const Game&)> onLaunch;
    std::function<void(const Game&)> onPin;
};

}
