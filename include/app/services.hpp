#pragma once

namespace akira::cloud {
class Service;
}

namespace akira::trophies {
class TrophyManager;
}

namespace akira::app {

struct Services {
    trophies::TrophyManager& trophies;
#if AKIRA_CLOUD
    cloud::Service& cloud;
#endif
};

}
