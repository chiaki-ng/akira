#ifndef AKIRA_WIREGUARD_MANAGER_HPP
#define AKIRA_WIREGUARD_MANAGER_HPP

#include <string>
#include <mutex>
#include <atomic>
#include <memory>
#include <map>
#include <vector>

#include <libchiaking/consoles_types.hpp>

#include "network/routing_network_provider.hpp"

extern "C" {
#include <wireguard.h>
}

namespace akira::play {
class ChiakiCore;
}

namespace akira::settings {
class DeveloperSettings;
}

namespace akira::network {

class WireGuardManager {
public:
    WireGuardManager(const settings::DeveloperSettings& developer, play::ChiakiCore& core);
    ~WireGuardManager();

    WireGuardManager(const WireGuardManager&) = delete;
    WireGuardManager& operator=(const WireGuardManager&) = delete;

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
    static void forwardLibraryLog(const char* message);

    static inline std::atomic<const settings::DeveloperSettings*> libraryLogSettings{nullptr};

    static constexpr const char* CONFIG_PATH = "sdmc:/switch/akira/wg0.conf";

    bool parseConfigFile(const std::string& path, WgConfig& config);
    std::string parseValue(const std::string& line);
    auto routesFor(const std::string& consoleAddress) const -> std::vector<RoutingNetworkProvider::Prefix>;

    play::ChiakiCore& core;
    WgTunnel* tunnel = nullptr;
    WgConfig config = {};
    bool configLoaded = false;
    bool connecting = false;
    std::atomic<bool> connected{false};
    std::string lastError;
    std::string tunnelIP;
    std::vector<RoutingNetworkProvider::Prefix> allowedIps;
    std::shared_ptr<TunnelStack> stack;
    mutable std::mutex mutex;
};

}

#endif
