#include "network/network_settings.hpp"
#include "network/custom_dns.hpp"
#include "settings/settings_store.hpp"
#include "log/log.hpp"

#include <utility>

namespace akira::network {

NetworkSettings::NetworkSettings(settings::SettingsStore& store, DnsListener dnsChanged)
    : store(store), dnsChanged(std::move(dnsChanged))
{
    read(store.loaded());
    store.attach(*this);
}

NetworkSettings::~NetworkSettings()
{
    store.detach(*this);
}

void NetworkSettings::read(const toml::table& config)
{
    const auto network = config["network"];
    if (auto val = network["holepunch_retry"].value<bool>())
        holepunch = *val;
    if (auto val = network["port_guessing"].value<bool>())
        guessPorts = *val;
    if (auto val = network["port_guessing_count"].value<int64_t>())
        guessCount = static_cast<int>(*val);
    if (auto val = network["port_guessing_socks"].value<int64_t>())
        guessSocks = static_cast<int>(*val);
    if (auto val = network["discovery_subnets"].value<std::string>())
        subnets = *val;
    if (auto val = network["custom_dns"].value<std::string>()) {
        if (auto servers = CustomDns::parseServers(*val))
            setCustomDnsServers(std::move(*servers));
        else
            log::warning("Ignoring invalid network.custom_dns \"{}\"", *val);
    }
    if (auto val = network["companion_port"].value<int64_t>())
        companion = static_cast<int>(*val);

    if (auto val = config["stream"]["auto_reconnect"].value<bool>())
        reconnect = *val;
}

void NetworkSettings::write(toml::table& config) const
{
    toml::table network;
    if (holepunch) network.insert("holepunch_retry", true);
    network.insert("port_guessing", guessPorts);
    network.insert("port_guessing_count", guessCount);
    network.insert("port_guessing_socks", guessSocks);
    if (!subnets.empty()) network.insert("discovery_subnets", subnets);
    if (!dnsServers.empty()) network.insert("custom_dns", CustomDns::formatServers(dnsServers));
    network.insert("companion_port", companion);
    config.insert("network", std::move(network));

    if (!reconnect)
        settings::SettingsStore::table(config, "stream").insert("auto_reconnect", false);
}

void NetworkSettings::setPortGuessingCount(int count)
{
    if (count > 0)
        guessCount = count;
}

void NetworkSettings::setPortGuessingSocks(int count)
{
    if (count > 0)
        guessSocks = count;
}

void NetworkSettings::setCustomDnsServers(std::vector<Ipv4Address> servers)
{
    dnsServers = std::move(servers);
    if (dnsChanged)
        dnsChanged(dnsServers);
}

void NetworkSettings::setCompanionPort(int port)
{
    if (port > 0 && port <= 65535)
        companion = port;
}

}
