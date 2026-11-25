#pragma once

#include <optional>
#include <string>

#include "play/stream_profile.hpp"

namespace akira::play {

class StreamRoute {
public:
    StreamRoute(std::optional<std::string> cloudName, bool remote, bool vpn, bool ps5);

    StreamProfile profile() const;
    std::string context() const;

private:
    std::optional<std::string> cloudName;
    bool remote;
    bool vpn;
    bool ps5;
};

}
