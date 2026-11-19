#include "network/wireguard_manager.hpp"
#include "settings/developer_settings.hpp"
#include "play/chiaki_core.hpp"
#include "network/tunnel_stack.hpp"
#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <sstream>
#include <cstring>
#include <charconv>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "log/log.hpp"

namespace akira::network {

void WireGuardManager::forwardLibraryLog(const char* message) {
    const settings::DeveloperSettings* developer = libraryLogSettings.load(std::memory_order_acquire);
    if (developer && developer->wireguardLog())
        log::info("WG-LIB: {}", message);
}

WireGuardManager::WireGuardManager(const settings::DeveloperSettings& developer) {
    memset(&config, 0, sizeof(config));
    libraryLogSettings.store(&developer, std::memory_order_release);
    wg_set_log_callback(forwardLibraryLog);
}

WireGuardManager::~WireGuardManager() {
    disconnect();
    libraryLogSettings.store(nullptr, std::memory_order_release);
}

bool WireGuardManager::configExists() const {
    std::ifstream file(CONFIG_PATH);
    return file.good();
}

std::string WireGuardManager::parseValue(const std::string& line) {
    size_t pos = line.find('=');
    if (pos == std::string::npos)
        return "";
    std::string value = line.substr(pos + 1);
    size_t start = value.find_first_not_of(" \t");
    size_t end = value.find_last_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    return value.substr(start, end - start + 1);
}

bool WireGuardManager::parseConfigFile(const std::string& path, WgConfig& cfg) {
    log::info("WG: parseConfigFile({})", path);
    std::ifstream file(path);
    if (!file.is_open()) {
        lastError = "Cannot open config file";
        log::error("WG: cannot open config file");
        return false;
    }
    log::info("WG: config file opened");

    std::string line;
    std::string section;
    std::string privateKey, publicKey, endpoint, address, presharedKey;
    uint16_t port = 51820;
    uint16_t keepalive = 25;
    uint16_t mtu = WG_DEFAULT_MTU;
    std::vector<RoutingNetworkProvider::Prefix> allowed;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;

        if (line[0] == '[') {
            size_t end = line.find(']');
            if (end != std::string::npos)
                section = line.substr(1, end - 1);
            continue;
        }

        std::string key = line.substr(0, line.find('='));
        size_t keyStart = key.find_first_not_of(" \t");
        size_t keyEnd = key.find_last_not_of(" \t");
        if (keyStart != std::string::npos)
            key = key.substr(keyStart, keyEnd - keyStart + 1);

        if (section == "Interface" && key == "MTU") {
            std::string value = parseValue(line);
            int parsed = 0;
            auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (error != std::errc{} || end != value.data() + value.size() ||
                parsed < WG_MIN_MTU || parsed > WG_MAX_MTU) {
                lastError = std::format("Invalid MTU: expected {}-{}", WG_MIN_MTU, WG_MAX_MTU);
                log::error("WG: {}", lastError);
                return false;
            }
            mtu = static_cast<uint16_t>(parsed);
            log::info("WG: configured MTU: {}", mtu);
        } else if (section == "Peer" && key == "AllowedIPs") {
            std::stringstream list(parseValue(line));
            std::string entry;
            while (std::getline(list, entry, ',')) {
                const auto start = entry.find_first_not_of(" \t");
                const auto end = entry.find_last_not_of(" \t\r\n");
                if (start == std::string::npos)
                    continue;
                const auto trimmed = entry.substr(start, end - start + 1);
                if (auto prefix = RoutingNetworkProvider::Prefix::parse(trimmed))
                    allowed.push_back(*prefix);
                else
                    log::info("WG: AllowedIPs entry {} is not routed through the tunnel", trimmed);
            }
        } else if (line.find("PrivateKey") != std::string::npos) {
            privateKey = parseValue(line);
            log::info("WG: found PrivateKey");
        } else if (line.find("PublicKey") != std::string::npos) {
            publicKey = parseValue(line);
            log::info("WG: found PublicKey");
        } else if (line.find("Endpoint") != std::string::npos) {
            std::string ep = parseValue(line);
            log::info("WG: found Endpoint: {}", ep);
            size_t colonPos = ep.rfind(':');
            if (colonPos != std::string::npos) {
                endpoint = ep.substr(0, colonPos);
                std::string portStr = ep.substr(colonPos + 1);
                log::info("WG: parsing port: {}", portStr);
                port = static_cast<uint16_t>(std::stoi(portStr));
            } else {
                endpoint = ep;
            }
            log::info("WG: endpoint={} port={}", endpoint, port);
        } else if (line.find("Address") != std::string::npos) {
            address = parseValue(line);
            log::info("WG: found Address: {}", address);
            size_t slashPos = address.find('/');
            if (slashPos != std::string::npos)
                address = address.substr(0, slashPos);
            size_t commaPos = address.find(',');
            if (commaPos != std::string::npos)
                address = address.substr(0, commaPos);
            log::info("WG: parsed address: {}", address);
        } else if (line.find("PresharedKey") != std::string::npos) {
            presharedKey = parseValue(line);
            log::info("WG: found PresharedKey");
        } else if (line.find("PersistentKeepalive") != std::string::npos) {
            std::string kaStr = parseValue(line);
            log::info("WG: found PersistentKeepalive: {}", kaStr);
            keepalive = static_cast<uint16_t>(std::stoi(kaStr));
        }
    }

    log::info("WG: validating parsed values");
    if (privateKey.empty()) {
        lastError = "Missing PrivateKey";
        log::error("WG: {}", lastError);
        return false;
    }
    if (publicKey.empty()) {
        lastError = "Missing PublicKey";
        log::error("WG: {}", lastError);
        return false;
    }
    if (endpoint.empty()) {
        lastError = "Missing Endpoint";
        log::error("WG: {}", lastError);
        return false;
    }
    if (address.empty()) {
        lastError = "Missing Address";
        log::error("WG: {}", lastError);
        return false;
    }

    log::info("WG: decoding private key");
    if (wg_key_from_base64(cfg.private_key, privateKey.c_str()) != 0) {
        lastError = "Invalid PrivateKey format";
        log::error("WG: {}", lastError);
        return false;
    }
    log::info("WG: decoding public key");
    if (wg_key_from_base64(cfg.peer_public_key, publicKey.c_str()) != 0) {
        lastError = "Invalid PublicKey format";
        log::error("WG: {}", lastError);
        return false;
    }

    log::info("WG: parsing tunnel IP: {}", address);
    if (inet_pton(AF_INET, address.c_str(), &cfg.tunnel_ip) != 1) {
        lastError = "Invalid Address format";
        log::error("WG: {}", lastError);
        return false;
    }

    log::info("WG: setting endpoint host: {}", endpoint);
    strncpy(cfg.endpoint_host, endpoint.c_str(), sizeof(cfg.endpoint_host) - 1);
    cfg.endpoint_port = port;
    cfg.keepalive_interval = keepalive;
    cfg.mtu = mtu;
    if (!presharedKey.empty()) {
        if (wg_key_from_base64(cfg.preshared_key, presharedKey.c_str()) != 0) {
            lastError = "Invalid PresharedKey format";
            log::error("WG: {}", lastError);
            return false;
        }
        cfg.has_preshared_key = 1;
        log::info("WG: PresharedKey configured");
    } else {
        cfg.has_preshared_key = 0;
    }

    tunnelIP = address;
    allowedIps = std::move(allowed);
    log::info("WG: parseConfigFile completed successfully");
    return true;
}

bool WireGuardManager::loadConfig() {
    std::lock_guard<std::mutex> lock(mutex);
    if (connecting)
        return false;
    if (!parseConfigFile(CONFIG_PATH, config))
        return false;
    configLoaded = true;
    return true;
}

bool WireGuardManager::connect() {
    log::info("WG: connect() entered, acquiring lock");
    {
        std::lock_guard<std::mutex> lock(mutex);
        log::info("WG: lock acquired");

        if (connected) {
            log::info("WG: already connected");
            return true;
        }

        if (connecting) {
            log::info("WG: a connect is already in progress");
            return false;
        }

        if (!configLoaded) {
            log::info("WG: parsing config file");
            if (!parseConfigFile(CONFIG_PATH, config)) {
                log::error("WG: parseConfigFile failed: {}", lastError);
                return false;
            }
            configLoaded = true;
            log::info("WG: config loaded successfully");
        }
        connecting = true;
    }

    std::string failure;
    std::shared_ptr<TunnelStack> opened;

    log::info("WG: calling wg_init");
    WgTunnel* const handshaking = wg_init(&config);
    if (!handshaking) {
        failure = "Failed to initialize tunnel";
        log::error("WG: wg_init failed");
    } else {
        log::info("WG: wg_init succeeded, calling wg_connect");
        int err = wg_connect(handshaking);
        if (err != WG_OK) {
            failure = std::format("Handshake failed: {}", err);
            log::error("WG: wg_connect failed: {}", err);
        } else {
            log::info("WG: wg_connect succeeded, calling wg_start");
            err = wg_start(handshaking);
            if (err != WG_OK) {
                failure = std::format("Failed to start tunnel: {}", err);
                log::error("WG: wg_start failed: {}", err);
            } else {
                std::array<uint8_t, 4> tunnelBytes{};
                std::memcpy(tunnelBytes.data(), &config.tunnel_ip, tunnelBytes.size());
                auto& core = play::ChiakiCore::current();
                auto started = TunnelStack::start(*handshaking,
                    libchiaking::platform::IpAddress::ipv4(tunnelBytes), core.threading(), core.clock());
                if (!started) {
                    failure = "Failed to start the tunnel's network stack";
                    log::error("WG: tunnel stack failed: {}", static_cast<int>(started.error()));
                    wg_stop(handshaking);
                } else {
                    opened = std::move(started).value();
                }
            }
        }
        if (!opened)
            wg_close(handshaking);
    }

    std::lock_guard<std::mutex> lock(mutex);
    connecting = false;
    if (!opened) {
        lastError = std::move(failure);
        return false;
    }
    tunnel = handshaking;
    stack = std::move(opened);
    log::info("WG: connected and started successfully");
    connected = true;
    lastError.clear();
    return true;
}

void WireGuardManager::disconnect() {
    std::lock_guard<std::mutex> lock(mutex);

    if (stack) {
        stack->stop();
        stack.reset();
    }
    if (tunnel) {
        wg_stop(tunnel);
        wg_close(tunnel);
        tunnel = nullptr;
    }
    connected = false;
}

bool WireGuardManager::isConnected() const {
    return connected;
}

std::string WireGuardManager::getTunnelIP() const {
    std::lock_guard<std::mutex> lock(mutex);
    return tunnelIP;
}

std::string WireGuardManager::getLastError() const {
    std::lock_guard<std::mutex> lock(mutex);
    return lastError;
}

auto WireGuardManager::routesFor(const std::string& consoleAddress) const
    -> std::vector<RoutingNetworkProvider::Prefix> {
    using Prefix = RoutingNetworkProvider::Prefix;
    std::vector<Prefix> routes = allowedIps;
    if (const auto address = libchiaking::platform::parse_ip_address(consoleAddress)) {
        if (auto host = Prefix::host(*address))
            routes.push_back(*host);
    }
    if (routes.empty()) {
        if (auto everything = Prefix::parse("0.0.0.0/0"))
            routes.push_back(*everything);
    }
    return routes;
}

auto WireGuardManager::homeNetwork(const std::string& consoleAddress) -> libchiaking::consoles::HomeNetwork {
    std::lock_guard<std::mutex> lock(mutex);
    if (!connected || !stack)
        return {};
    auto& core = play::ChiakiCore::current();
    return libchiaking::consoles::HomeNetwork{std::make_shared<RoutingNetworkProvider>(
        core.network(), core.threading(), core.clock(), stack, routesFor(consoleAddress))};
}

auto WireGuardManager::discoveryNetwork() -> libchiaking::consoles::HomeNetwork {
    std::lock_guard<std::mutex> lock(mutex);
    if (!connected || !stack)
        return {};
    auto& core = play::ChiakiCore::current();
    return libchiaking::consoles::HomeNetwork{
        RoutingNetworkProvider::tunnelOnly(core.network(), core.threading(), core.clock(), stack)};
}

auto WireGuardManager::tunnels(const std::string& consoleAddress) const -> bool {
    const auto address = libchiaking::platform::parse_ip_address(consoleAddress);
    if (!address || !address->is_ipv4() || address->is_unspecified() || address->is_loopback())
        return false;
    std::lock_guard<std::mutex> lock(mutex);
    if (!connected || !stack)
        return false;
    return std::ranges::any_of(routesFor(consoleAddress), [&address](const auto& prefix) {
        return prefix.contains(*address);
    });
}

auto WireGuardManager::allowedHosts() const -> std::vector<libchiaking::platform::IpAddress> {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<libchiaking::platform::IpAddress> hosts;
    if (!connected || !stack)
        return hosts;
    for (const auto& prefix : allowedIps) {
        if (auto host = prefix.singleHost())
            hosts.push_back(*host);
    }
    return hosts;
}

}
