#pragma once

#include <cstddef>
#include <functional>
#include <vector>

#include "app/views/vendored/switchfin/recycling_grid.hpp"
#include "cloud/models.hpp"
#include "cloud/views/cloud_game_card.hpp"

namespace akira::cloud::views {

class CloudGameRowCell : public app::views::RecyclingGridItem {
public:
    static constexpr size_t PerRow = 4;
    static constexpr float Height = 258.0f;

    CloudGameRowCell();

    static app::views::RecyclingGridItem* create();

    void bindRow(const std::vector<Game>& games, size_t row,
        const std::function<void(const Game&)>& onLaunch,
        const std::function<void(const Game&)>& onPin);
    void prepareForReuse() override;

private:
    CloudGameCard* cards[PerRow] = {};
};

}
