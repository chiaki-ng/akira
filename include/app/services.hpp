#pragma once

namespace akira::cloud {
class Service;
}

namespace akira::consoles {
class DiscoveryManager;
}

namespace akira::trophies {
class TrophyManager;
}

namespace akira::app {

struct Services {
    consoles::DiscoveryManager& discovery;
    trophies::TrophyManager& trophies;
#if AKIRA_CLOUD
    cloud::Service& cloud;
#endif
};

}
