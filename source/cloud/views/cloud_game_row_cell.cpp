#include "cloud/views/cloud_game_row_cell.hpp"

#include "cloud/catalog_rows.hpp"

namespace akira::cloud::views {

CloudGameRowCell::CloudGameRowCell()
{
    this->setAxis(brls::Axis::ROW);
    this->setWidthPercentage(100.0f);
    this->setFocusable(false);

    for (size_t i = 0; i < PerRow; i++)
    {
        cards[i] = new CloudGameCard();
        cards[i]->setGrow(1.0f);
        if (i > 0)
            cards[i]->setMarginLeft(16);
        this->addView(cards[i]);
    }
}

app::views::RecyclingGridItem* CloudGameRowCell::create()
{
    return new CloudGameRowCell();
}

void CloudGameRowCell::bindRow(const std::vector<Game>& games, size_t row,
    const std::function<void(const Game&)>& onLaunch,
    const std::function<void(const Game&)>& onPin)
{
    const CatalogRows rows(games.size(), PerRow);
    for (size_t i = 0; i < PerRow; i++)
    {
        if (const auto idx = rows.gameAt(row, i))
            cards[i]->bind(games[*idx], onLaunch, onPin);
        else
            cards[i]->reset();
    }
}

void CloudGameRowCell::prepareForReuse()
{
    for (size_t i = 0; i < PerRow; i++)
        if (cards[i])
            cards[i]->reset();
}

}
