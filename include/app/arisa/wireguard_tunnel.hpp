#pragma once

#include "app/services.hpp"
#include "network/tunnel.hpp"

namespace akira::app::arisa {

class WireGuardTunnel final : public network::Tunnel {
public:
    explicit WireGuardTunnel(Services& services);

    auto configured() const -> bool override;
    auto connected() const -> bool override;
    auto address() const -> std::string override;
    auto lastError() const -> std::string override;
    auto connect() -> bool override;
    void disconnect() override;
    void restartSearch() override;

private:
    Services& services;
};

}
