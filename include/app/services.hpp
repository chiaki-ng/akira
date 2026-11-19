#pragma once

namespace akira::trophies {
class TrophyManager;
}

namespace akira::app {

struct Services {
    trophies::TrophyManager& trophies;
};

}
