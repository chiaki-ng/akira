#include "cloud/game_badges.hpp"

namespace akira::cloud {

GameBadges::GameBadges(const Game& game)
    : streamable(game.streamableNow() && !game.launchLeftOut)
    , launchLeftOut(game.launchLeftOut)
{
    std::string platform = platformBadge(game);
    if (!platform.empty())
    {
        const Kind kind = game.platform == "ps5" ? Kind::PlatformPs5
                        : game.platform == "ps4" ? Kind::PlatformPs4
                                                 : Kind::PlatformOther;
        shown.push_back({kind, std::move(platform)});
    }
    if (game.plusCatalog)
        shown.push_back({Kind::Plus, {}});
    if (game.serviceType == "pscloud")
    {
        if (game.streamabilityStatus == StreamabilityStatus::Streamable)
            shown.push_back({Kind::Verified, {}});
        else if (game.streamabilityStatus == StreamabilityStatus::NotStreamable)
            shown.push_back({Kind::Unavailable, {}});
        else
            shown.push_back({Kind::Unverified, {}});
    }
    std::string category = categoryBadge(game);
    if (!category.empty())
    {
        const Kind kind = game.isOwned ? Kind::CategoryOwned
                        : game.category == "streamable" ? Kind::CategoryCatalog
                                                        : Kind::CategoryOther;
        shown.push_back({kind, std::move(category)});
    }
}

}
