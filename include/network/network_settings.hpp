#pragma once

#include <functional>
#include <string>
#include <vector>

#include "network/ipv4_address.hpp"
#include "settings/section.hpp"

namespace akira::settings {
class SettingsStore;
}

namespace akira::network {

class NetworkSettings final : public settings::Section {
public:
    using DnsListener = std::function<void(const std::vector<Ipv4Address>&)>;

    NetworkSettings(settings::SettingsStore& store, DnsListener dnsChanged);
    ~NetworkSettings() override;

    NetworkSettings(const NetworkSettings&) = delete;
    NetworkSettings& operator=(const NetworkSettings&) = delete;

    void write(toml::table& config) const override;

    bool holepunchRetry() const { return holepunch; }
    void setHolepunchRetry(bool retry) { holepunch = retry; }
    bool portGuessing() const { return guessPorts; }
    void setPortGuessing(bool enabled) { guessPorts = enabled; }
    int portGuessingCount() const { return guessCount; }
    void setPortGuessingCount(int count);
    int portGuessingSocks() const { return guessSocks; }
    void setPortGuessingSocks(int count);

    const std::string& discoverySubnets() const { return subnets; }
    void setDiscoverySubnets(const std::string& value) { subnets = value; }
    const std::vector<Ipv4Address>& customDnsServers() const { return dnsServers; }
    void setCustomDnsServers(std::vector<Ipv4Address> servers);

    bool autoReconnect() const { return reconnect; }
    void setAutoReconnect(bool enabled) { reconnect = enabled; }

    int companionPort() const { return companion; }
    void setCompanionPort(int port);

private:
    void read(const toml::table& config);

    settings::SettingsStore& store;
    DnsListener dnsChanged;

    bool holepunch = false;
    bool guessPorts = true;
    int guessCount = 75;
    int guessSocks = 120;
    std::string subnets;
    std::vector<Ipv4Address> dnsServers;
    bool reconnect = true;
    int companion = 8080;
};

}
