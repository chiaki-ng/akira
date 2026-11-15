#ifndef AKIRA_CUSTOM_DNS_HPP
#define AKIRA_CUSTOM_DNS_HPP

#include <netdb.h>
#include <netinet/in.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "util/dns_client.hpp"
#include "util/ipv4_address.hpp"

namespace akira::net {

class CustomDns {
public:
    static constexpr std::size_t MaxServers = 2;
    static constexpr std::array<Ipv4Address, 2> DefaultServers{
        Ipv4Address::fromNetworkOrder(0x01010101), Ipv4Address::fromNetworkOrder(0x08080808)};
    static constexpr std::array<std::string_view, 4> SonyDomains{
        "playstation.com", "playstation.net", "sonyentertainmentnetwork.com", "sony.com"};

    static bool isSonyHost(std::string_view host)
    {
        if (host.ends_with('.'))
            host.remove_suffix(1);
        return std::ranges::any_of(SonyDomains, [host](std::string_view domain) {
            return host.ends_with(domain) && (host.size() == domain.size() || host[host.size() - domain.size() - 1] == '.');
        });
    }

    static CustomDns& instance();
    static std::optional<std::vector<Ipv4Address>> parseServers(std::string_view text);
    static std::string formatServers(std::span<const Ipv4Address> servers);

    void configure(std::vector<Ipv4Address> servers);
    std::vector<Ipv4Address> servers() const;
    std::vector<Ipv4Address> resolvers() const;

    std::optional<int> getaddrinfo(const char* node, const char* service, const addrinfo* hints, addrinfo** result);
    bool freeaddrinfo(addrinfo* list);

    CustomDns(const CustomDns&) = delete;
    CustomDns& operator=(const CustomDns&) = delete;

private:
    struct CacheEntry {
        std::vector<Ipv4Address> addresses;
        std::chrono::steady_clock::time_point expires;
    };

    struct OwnedList {
        std::vector<addrinfo> nodes;
        std::vector<sockaddr_in> addresses;
        std::string canonicalName;
    };

    static constexpr std::chrono::seconds MinTtl{30};
    static constexpr std::chrono::seconds MaxTtl{3600};

    CustomDns() = default;

    std::expected<std::vector<Ipv4Address>, int> resolve(const std::string& name);
    std::optional<std::vector<Ipv4Address>> cached(const std::string& name);
    addrinfo* adopt(std::unique_ptr<OwnedList> list);
    static std::optional<std::uint16_t> parsePort(const char* service);
    static std::unique_ptr<OwnedList> buildList(std::span<const Ipv4Address> addresses, std::uint16_t port, const addrinfo* hints,
        std::string_view name);

    mutable std::mutex m_mutex;
    std::vector<Ipv4Address> m_servers;
    std::unordered_map<std::string, CacheEntry> m_cache;
    std::unordered_map<addrinfo*, std::unique_ptr<OwnedList>> m_owned;
    DnsClient m_client{std::chrono::milliseconds(1000)};
};

} // namespace akira::net

#endif // AKIRA_CUSTOM_DNS_HPP
