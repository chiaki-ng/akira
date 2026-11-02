#ifndef AKIRA_WIREGUARD_MANAGER_HPP
#define AKIRA_WIREGUARD_MANAGER_HPP

#include <string>
#include <mutex>
#include <atomic>
#include <memory>
#include <map>
#include <vector>

#include <libchiaking/consoles_types.hpp>

#include "core/net/routing_network_provider.hpp"

extern "C" {
#include <wireguard.h>
}

class WireGuardManager {
public:
    static WireGuardManager& instance();

    bool configExists() const;
    bool loadConfig();
    bool connect();
    void disconnect();
    bool isConnected() const;
    std::string getTunnelIP() const;
    std::string getLastError() const;
    auto homeNetwork(const std::string& consoleAddress) -> libchiaking::consoles::HomeNetwork;
    auto discoveryNetwork() -> libchiaking::consoles::HomeNetwork;
    auto tunnels(const std::string& consoleAddress) const -> bool;
    auto allowedHosts() const -> std::vector<libchiaking::platform::IpAddress>;

private:
    WireGuardManager();
    ~WireGuardManager();
    WireGuardManager(const WireGuardManager&) = delete;
    WireGuardManager& operator=(const WireGuardManager&) = delete;

    static constexpr const char* CONFIG_PATH = "sdmc:/switch/akira/wg0.conf";

    bool parseConfigFile(const std::string& path, WgConfig& config);
    std::string parseValue(const std::string& line);
    auto routesFor(const std::string& consoleAddress) const -> std::vector<akira::net::RoutingNetworkProvider::Prefix>;

    WgTunnel* tunnel = nullptr;
    WgConfig config = {};
    bool configLoaded = false;
    bool connecting = false;
    std::atomic<bool> connected{false};
    std::string lastError;
    std::string tunnelIP;
    std::vector<akira::net::RoutingNetworkProvider::Prefix> allowedIps;
    std::shared_ptr<akira::net::TunnelStack> stack;
    mutable std::mutex mutex;
};

#endif
