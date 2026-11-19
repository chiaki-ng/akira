#pragma once

#include <string>
#include <vector>

#include "cloud/models.hpp"

namespace akira::cloud {

class GameBadges {
public:
    enum class Kind {
        PlatformPs5,
        PlatformPs4,
        PlatformOther,
        Plus,
        Verified,
        Unavailable,
        Unverified,
        CategoryOwned,
        CategoryCatalog,
        CategoryOther
    };

    struct Badge {
        Kind kind;
        std::string label;
    };

    explicit GameBadges(const Game& game);

    auto badges() const -> const std::vector<Badge>& { return shown; }
    bool playable() const { return streamable; }
    bool leftOut() const { return launchLeftOut; }

private:
    std::vector<Badge> shown;
    bool streamable = false;
    bool launchLeftOut = false;
};

}
