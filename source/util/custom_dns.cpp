#include "util/custom_dns.hpp"

#include <borealis.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <ranges>

namespace akira::net {

CustomDns& CustomDns::instance()
{
    static CustomDns dns;
    return dns;
}

std::optional<std::vector<Ipv4Address>> CustomDns::parseServers(std::string_view text)
{
    std::vector<Ipv4Address> servers;
    for (const auto part : std::views::split(text, ',')) {
        std::string_view entry(part.begin(), part.end());
        while (!entry.empty() && entry.front() == ' ')
            entry.remove_prefix(1);
        while (!entry.empty() && entry.back() == ' ')
            entry.remove_suffix(1);
        if (entry.empty())
            continue;
        const auto address = Ipv4Address::parse(entry);
        if (!address || address->isUnspecified() || servers.size() == MaxServers)
            return std::nullopt;
        if (std::ranges::find(servers, *address) == servers.end())
            servers.push_back(*address);
    }
    return servers;
}

std::string CustomDns::formatServers(std::span<const Ipv4Address> servers)
{
    std::string text;
    for (const Ipv4Address& server : servers) {
        if (!text.empty())
            text += ", ";
        text += server.toString();
    }
    return text;
}

void CustomDns::configure(std::vector<Ipv4Address> servers)
{
    std::lock_guard lock(m_mutex);
    if (servers == m_servers)
        return;
    m_servers = std::move(servers);
    m_cache.clear();
    brls::Logger::info("[NET] PlayStation names resolve in akira through {}{}",
        formatServers(m_servers.empty() ? std::span<const Ipv4Address>(DefaultServers) : std::span<const Ipv4Address>(m_servers)),
        m_servers.empty() ? " (built-in)" : "");
}

std::vector<Ipv4Address> CustomDns::servers() const
{
    std::lock_guard lock(m_mutex);
    return m_servers;
}

std::vector<Ipv4Address> CustomDns::resolvers() const
{
    std::lock_guard lock(m_mutex);
    if (m_servers.empty())
        return {DefaultServers.begin(), DefaultServers.end()};
    return m_servers;
}

std::optional<std::uint16_t> CustomDns::parsePort(const char* service)
{
    if (!service || !*service)
        return std::uint16_t{0};
    const std::string_view text(service);
    if (text == "http")
        return std::uint16_t{80};
    if (text == "https")
        return std::uint16_t{443};
    std::uint16_t port = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), port);
    if (error != std::errc{} || end != text.data() + text.size())
        return std::nullopt;
    return port;
}

std::optional<std::vector<Ipv4Address>> CustomDns::cached(const std::string& name)
{
    std::lock_guard lock(m_mutex);
    const auto entry = m_cache.find(name);
    if (entry == m_cache.end())
        return std::nullopt;
    if (entry->second.expires <= std::chrono::steady_clock::now()) {
        m_cache.erase(entry);
        return std::nullopt;
    }
    return entry->second.addresses;
}

std::expected<std::vector<Ipv4Address>, int> CustomDns::resolve(const std::string& name)
{
    if (auto hit = cached(name))
        return std::move(*hit);

    int error = EAI_AGAIN;
    for (const Ipv4Address& server : resolvers()) {
        auto answer = m_client.queryA(server, name);
        if (!answer) {
            brls::Logger::warning("[NET] custom DNS {} via {} failed: {} errno={}", name, server.toString(),
                DnsClient::stepName(answer.error().step), answer.error().osErrno);
            if (answer.error().step == DnsClient::Step::NoSuchName) {
                error = EAI_NONAME;
                break;
            }
            continue;
        }
        if (answer->empty()) {
            error = EAI_NONAME;
            break;
        }
        const auto ttl = std::ranges::min(*answer | std::views::transform(&DnsClient::Record::ttl));
        std::vector<Ipv4Address> addresses;
        std::ranges::transform(*answer, std::back_inserter(addresses), &DnsClient::Record::address);
        std::lock_guard lock(m_mutex);
        m_cache[name] = {addresses, std::chrono::steady_clock::now() + std::clamp(ttl, MinTtl, MaxTtl)};
        return addresses;
    }
    return std::unexpected(error);
}

std::unique_ptr<CustomDns::OwnedList> CustomDns::buildList(std::span<const Ipv4Address> addresses, std::uint16_t port,
    const addrinfo* hints, std::string_view name)
{
    const int requestedType = hints ? hints->ai_socktype : 0;
    const int requestedProtocol = hints ? hints->ai_protocol : 0;
    const bool canonical = hints && (hints->ai_flags & AI_CANONNAME);
    std::vector<int> types;
    if (requestedType != 0)
        types.push_back(requestedType);
    else
        types = {SOCK_STREAM, SOCK_DGRAM};

    auto list = std::make_unique<OwnedList>();
    list->canonicalName = name;
    list->addresses.reserve(addresses.size() * types.size());
    list->nodes.reserve(addresses.size() * types.size());
    for (const Ipv4Address& address : addresses) {
        for (const int type : types) {
            sockaddr_in& socketAddress = list->addresses.emplace_back();
            socketAddress.sin_family = AF_INET;
            socketAddress.sin_port = htons(port);
            socketAddress.sin_addr.s_addr = address.networkOrder();
            addrinfo& node = list->nodes.emplace_back();
            node.ai_family = AF_INET;
            node.ai_socktype = type;
            node.ai_protocol = requestedProtocol != 0 ? requestedProtocol : (type == SOCK_DGRAM ? IPPROTO_UDP : IPPROTO_TCP);
            node.ai_addrlen = sizeof(sockaddr_in);
            node.ai_addr = reinterpret_cast<sockaddr*>(&socketAddress);
        }
    }
    for (std::size_t i = 0; i + 1 < list->nodes.size(); ++i)
        list->nodes[i].ai_next = &list->nodes[i + 1];
    if (canonical && !list->nodes.empty())
        list->nodes.front().ai_canonname = list->canonicalName.data();
    return list;
}

addrinfo* CustomDns::adopt(std::unique_ptr<OwnedList> list)
{
    addrinfo* head = &list->nodes.front();
    std::lock_guard lock(m_mutex);
    m_owned.emplace(head, std::move(list));
    return head;
}

std::optional<int> CustomDns::getaddrinfo(const char* node, const char* service, const addrinfo* hints, addrinfo** result)
{
    if (!result || !node || !*node)
        return std::nullopt;
    std::string name(node);
    std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (!isSonyHost(name))
        return std::nullopt;
    if (hints && hints->ai_family != AF_UNSPEC && hints->ai_family != AF_INET)
        return EAI_FAMILY;
    const auto port = parsePort(service);
    if (!port)
        return EAI_SERVICE;
    auto addresses = resolve(name);
    if (!addresses)
        return addresses.error();
    *result = adopt(buildList(*addresses, *port, hints, name));
    return 0;
}

bool CustomDns::freeaddrinfo(addrinfo* list)
{
    std::lock_guard lock(m_mutex);
    return m_owned.erase(list) > 0;
}

} // namespace akira::net
