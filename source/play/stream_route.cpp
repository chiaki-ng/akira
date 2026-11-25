#include "play/stream_route.hpp"

#include <format>
#include <utility>

namespace akira::play {

StreamRoute::StreamRoute(std::optional<std::string> cloudName, bool remote, bool vpn, bool ps5)
    : cloudName(std::move(cloudName))
    , remote(remote)
    , vpn(vpn)
    , ps5(ps5)
{
}

StreamProfile StreamRoute::profile() const
{
    if (cloudName)
        return StreamProfile::Cloud;
    if (remote)
        return StreamProfile::Remote;
    if (vpn)
        return StreamProfile::Vpn;
    return StreamProfile::Local;
}

std::string StreamRoute::context() const
{
    const char* platformLabel = ps5 ? "PS5" : "PS4";
    if (cloudName)
        return std::format("Cloud \xc2\xb7 {}", *cloudName);
    if (remote)
        return std::format("{} \xc2\xb7 Remote", platformLabel);
    if (vpn)
        return std::format("{} \xc2\xb7 VPN", platformLabel);
    return std::format("{} \xc2\xb7 Direct", platformLabel);
}

}
