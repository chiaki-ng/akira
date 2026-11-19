#pragma once

#include <string>
#include <vector>

#include "consoles/host_type.hpp"

namespace akira::consoles {

class DebugConsoles {
public:
    struct Console {
        std::string nickname;
        bool resting = false;
        HostType type = HostType::Manual;
    };

    static constexpr int Count = 20;

    static std::vector<Console> list();
};

}
