#include "consoles/discovery_gate.hpp"

namespace akira::consoles {

DiscoveryGate::Discovered DiscoveryGate::discovered(bool hostPresent, bool listPresent) const
{
    if (!hostPresent)
        return Discovered::NoHost;
    if (!open())
        return Discovered::Suspended;
    if (!listPresent)
        return Discovered::NoList;
    return Discovered::Update;
}

}
