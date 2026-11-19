#pragma once

#include <string>

#include "consoles/host_type.hpp"

namespace akira::consoles {

struct ConsoleFacts {
    std::string catalogName;
    std::string name;
    HostType type = HostType::Discovered;
    bool cloud = false;
    bool paired = false;
    bool discovered = false;
    bool needsLink = false;
    bool ps5 = false;
    bool standby = false;
    bool ready = false;
    bool canPairOverInternet = false;
    bool hasAddress = false;
    std::string presence;
};

}
