#pragma once

namespace akira::cloud {
class Service;
}

namespace akira::consoles {
class DiscoveryManager;
}

namespace akira::platform {
class PlatformFactory;
}

namespace akira::play {
class LazySession;
class StreamPreferences;
}

namespace akira::trophies {
class TrophyManager;
}

namespace akira::app {

struct Services {
    platform::PlatformFactory& platform;
    consoles::DiscoveryManager& discovery;
    play::StreamPreferences& preferences;
    play::LazySession& session;
    trophies::TrophyManager& trophies;
#if AKIRA_CLOUD
    cloud::Service& cloud;
#endif
};

}
