#include "cloud/cloud_shortcuts.hpp"

#include <algorithm>
#include <utility>

namespace akira::cloud {

CloudShortcuts::CloudShortcuts(std::vector<Game> games)
    : pinned(std::move(games))
{
}

auto CloudShortcuts::toggle(const Game& game) -> Toggle
{
    if (!game.launchable())
        return Toggle::Ignored;
    auto it = std::find_if(pinned.begin(), pinned.end(),
        [&](const Game& g) { return g.productId == game.productId; });
    if (it != pinned.end())
    {
        pinned.erase(it);
        return Toggle::Removed;
    }
    if (game.launchLeftOut)
        return Toggle::NotBuilt;
    if (!game.streamableNow())
        return Toggle::NeedsLibrary;
    pinned.push_back(game);
    return Toggle::Added;
}

}
