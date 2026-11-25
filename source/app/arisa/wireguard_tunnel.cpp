#include "app/arisa/wireguard_tunnel.hpp"

#include "consoles/discovery_manager.hpp"
#include "network/wireguard_manager.hpp"

namespace akira::app::arisa {

WireGuardTunnel::WireGuardTunnel(Services& services)
    : services(services)
{
}

auto WireGuardTunnel::configured() const -> bool
{
    return services.wireGuard.configExists();
}

auto WireGuardTunnel::connected() const -> bool
{
    return services.wireGuard.isConnected();
}

auto WireGuardTunnel::address() const -> std::string
{
    return services.wireGuard.getTunnelIP();
}

auto WireGuardTunnel::lastError() const -> std::string
{
    return services.wireGuard.getLastError();
}

auto WireGuardTunnel::connect() -> bool
{
    return services.wireGuard.connect();
}

void WireGuardTunnel::disconnect()
{
    services.wireGuard.disconnect();
}

void WireGuardTunnel::restartSearch()
{
    services.discovery.restartSearch();
}

}
