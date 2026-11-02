#include "core/net/routing_network_provider.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <utility>
#include <variant>

#include "core/net/tunnel_stack.hpp"

namespace akira::net {

namespace platform = libchiaking::platform;
using libchiaking::Expected;
using libchiaking::unexpected;
using platform::PollEvent;
using platform::SocketEndpoint;

class RoutingNetworkProvider::Routed {
public:
    using Target = std::variant<const platform::Pollable*, const TunnelStack::Endpoint*>;

    virtual auto target() const -> Target = 0;

protected:
    Routed() = default;
    ~Routed() = default;
    Routed(const Routed&) = delete;
    Routed& operator=(const Routed&) = delete;
};

class RoutingNetworkProvider::UdpSocket final : public platform::UdpSocket, public Routed {
public:
    UdpSocket(RoutingNetworkProvider& network, bool ipv4, std::unique_ptr<platform::UdpSocket> lan)
        : network(network)
        , ipv4(ipv4)
        , mutex(network.threading)
        , route(std::move(lan))
    {
    }

    UdpSocket(RoutingNetworkProvider& network, std::unique_ptr<TunnelStack::Datagram> tunnel)
        : network(network)
        , ipv4(true)
        , mutex(network.threading)
        , decided(true)
        , route(std::move(tunnel))
    {
    }

    ~UdpSocket() override = default;

    auto owner() const noexcept -> const platform::NetworkProvider& override
    {
        return network;
    }

    auto target() const -> Target override
    {
        platform::Lock lock(mutex);
        decided = true;
        if (const auto* tunnel = std::get_if<Tunnel>(&route))
            return Target{tunnel->get()};
        return Target{std::get<Lan>(route).get()};
    }

    auto bind(const SocketEndpoint& endpoint) -> Expected<void, NetError> override
    {
        platform::Lock lock(mutex);
        if (auto* tunnel = std::get_if<Tunnel>(&route))
            return (*tunnel)->bind(endpoint.port);
        auto bound = std::get<Lan>(route)->bind(endpoint);
        if (bound)
            carry.port = endpoint.port;
        return bound;
    }

    auto send_to(std::span<const std::byte> datagram, const SocketEndpoint& destination)
        -> Expected<std::size_t, NetError> override
    {
        platform::Lock lock(mutex);
        if (auto routed = routeFor(destination); !routed)
            return unexpected(routed.error());
        if (auto* tunnel = std::get_if<Tunnel>(&route))
            return (*tunnel)->sendTo(datagram, destination);
        return std::get<Lan>(route)->send_to(datagram, destination);
    }

    auto receive_from(std::span<std::byte> buffer) -> Expected<platform::UdpReceiveResult, NetError> override
    {
        platform::Lock lock(mutex);
        decided = true;
        if (auto* tunnel = std::get_if<Tunnel>(&route))
            return (*tunnel)->receive(buffer);
        return std::get<Lan>(route)->receive_from(buffer);
    }

    auto connect(const SocketEndpoint& endpoint) -> Expected<void, NetError> override
    {
        platform::Lock lock(mutex);
        if (auto routed = routeFor(endpoint); !routed)
            return routed;
        if (auto* tunnel = std::get_if<Tunnel>(&route))
            return (*tunnel)->connect(endpoint);
        return std::get<Lan>(route)->connect(endpoint);
    }

    auto send(std::span<const std::byte> datagram) -> Expected<std::size_t, NetError> override
    {
        platform::Lock lock(mutex);
        if (auto* tunnel = std::get_if<Tunnel>(&route))
            return (*tunnel)->send(datagram);
        return std::get<Lan>(route)->send(datagram);
    }

    auto set_boolean_option(platform::SocketOption option, bool enabled) -> Expected<void, NetError> override
    {
        if (option == platform::SocketOption::broadcast)
            return unexpected(NetError::not_supported);
        platform::Lock lock(mutex);
        if (auto* tunnel = std::get_if<Tunnel>(&route))
        {
            if (option != platform::SocketOption::dont_fragment)
                return {};
            if (!dontFragmentAvailable())
                return unexpected(NetError::not_supported);
            return (*tunnel)->setDontFragment(enabled);
        }
        auto set = std::get<Lan>(route)->set_boolean_option(option, enabled);
        if (set && option == platform::SocketOption::dont_fragment)
            carry.dontFragment = enabled;
        return set;
    }

    auto set_shared_port_binding(bool enabled) -> Expected<void, NetError> override
    {
        platform::Lock lock(mutex);
        if (std::holds_alternative<Tunnel>(route))
            return {};
        return std::get<Lan>(route)->set_shared_port_binding(enabled);
    }

    auto set_receive_buffer_size(std::size_t bytes) -> Expected<void, NetError> override
    {
        platform::Lock lock(mutex);
        if (auto* tunnel = std::get_if<Tunnel>(&route))
            return (*tunnel)->setReceiveBuffer(bytes);
        auto set = std::get<Lan>(route)->set_receive_buffer_size(bytes);
        if (set)
            carry.receiveBuffer = bytes;
        return set;
    }

    auto receive_buffer_size() -> Expected<std::size_t, NetError> override
    {
        platform::Lock lock(mutex);
        if (auto* tunnel = std::get_if<Tunnel>(&route))
            return (*tunnel)->receiveBuffer();
        return std::get<Lan>(route)->receive_buffer_size();
    }

    auto set_ipv4_ttl(std::uint8_t ttl) -> Expected<void, NetError> override
    {
        platform::Lock lock(mutex);
        if (auto* tunnel = std::get_if<Tunnel>(&route))
            return (*tunnel)->setTtl(ttl);
        auto set = std::get<Lan>(route)->set_ipv4_ttl(ttl);
        if (set)
            carry.ttl = ttl;
        return set;
    }

    auto receive_traffic_class(bool enabled) -> Expected<void, NetError> override
    {
        platform::Lock lock(mutex);
        if (std::holds_alternative<Tunnel>(route))
            return {};
        return std::get<Lan>(route)->receive_traffic_class(enabled);
    }

    auto local_endpoint() -> Expected<SocketEndpoint, NetError> override
    {
        platform::Lock lock(mutex);
        if (auto* tunnel = std::get_if<Tunnel>(&route))
            return (*tunnel)->localEndpoint();
        return std::get<Lan>(route)->local_endpoint();
    }

    auto close() -> Expected<void, NetError> override
    {
        platform::Lock lock(mutex);
        decided = true;
        if (auto* tunnel = std::get_if<Tunnel>(&route))
            return (*tunnel)->close();
        return std::get<Lan>(route)->close();
    }

private:
    using Lan = std::unique_ptr<platform::UdpSocket>;
    using Tunnel = std::unique_ptr<TunnelStack::Datagram>;

    struct Carry {
        std::uint16_t port = 0;
        bool dontFragment = false;
        std::optional<std::size_t> receiveBuffer;
        std::optional<std::uint8_t> ttl;
    };

    auto dontFragmentAvailable() const noexcept -> bool
    {
        const auto available = network.capabilities();
        return ipv4 ? available.socket_dont_fragment_ipv4 : available.socket_dont_fragment_ipv6;
    }

    auto routeFor(const SocketEndpoint& destination) -> Expected<void, NetError>
    {
        if (decided)
            return {};
        if (!ipv4 || !network.tunnels(destination.address))
        {
            decided = true;
            return {};
        }
        auto opened = TunnelStack::Datagram::open(network.tunnel);
        if (!opened)
            return unexpected(opened.error());
        auto& datagram = *opened.value();
        if (carry.dontFragment)
        {
            if (auto set = datagram.setDontFragment(true); !set)
                return set;
        }
        if (carry.receiveBuffer)
        {
            if (auto set = datagram.setReceiveBuffer(*carry.receiveBuffer); !set)
                return set;
        }
        if (carry.ttl)
        {
            if (auto set = datagram.setTtl(*carry.ttl); !set)
                return set;
        }
        if (auto bound = datagram.bind(carry.port); !bound)
            return bound;
        static_cast<void>(std::get<Lan>(route)->close());
        route = std::move(opened).value();
        decided = true;
        return {};
    }

    RoutingNetworkProvider& network;
    const bool ipv4;
    mutable platform::Mutex mutex;
    mutable bool decided = false;
    std::variant<Lan, Tunnel> route;
    Carry carry;
};

class RoutingNetworkProvider::TcpSocket final : public platform::TcpSocket, public Routed {
public:
    TcpSocket(RoutingNetworkProvider& network, bool ipv4, std::unique_ptr<platform::TcpSocket> lan, bool decided)
        : network(network)
        , ipv4(ipv4)
        , mutex(network.threading)
        , decided(decided)
        , route(std::move(lan))
    {
    }

    TcpSocket(RoutingNetworkProvider& network, std::unique_ptr<TunnelStack::Stream> tunnel)
        : network(network)
        , ipv4(true)
        , mutex(network.threading)
        , decided(true)
        , route(std::move(tunnel))
    {
    }

    ~TcpSocket() override = default;

    auto owner() const noexcept -> const platform::NetworkProvider& override
    {
        return network;
    }

    auto target() const -> Target override
    {
        platform::Lock lock(mutex);
        decided = true;
        if (const auto* tunnel = std::get_if<Tunnel>(&route))
            return Target{tunnel->get()};
        return Target{std::get<Lan>(route).get()};
    }

    auto bind(const SocketEndpoint& endpoint) -> Expected<void, NetError> override
    {
        platform::Lock lock(mutex);
        if (auto* tunnel = std::get_if<Tunnel>(&route))
            return (*tunnel)->bind(endpoint.port);
        auto bound = std::get<Lan>(route)->bind(endpoint);
        if (bound)
            port = endpoint.port;
        return bound;
    }

    auto connect(const SocketEndpoint& endpoint) -> Expected<void, NetError> override
    {
        platform::Lock lock(mutex);
        if (auto routed = routeFor(endpoint); !routed)
            return routed;
        if (auto* tunnel = std::get_if<Tunnel>(&route))
            return (*tunnel)->connect(endpoint);
        return std::get<Lan>(route)->connect(endpoint);
    }

    auto complete_connect() -> Expected<void, NetError> override
    {
        platform::Lock lock(mutex);
        if (auto* tunnel = std::get_if<Tunnel>(&route))
            return (*tunnel)->completeConnect();
        return std::get<Lan>(route)->complete_connect();
    }

    auto send(std::span<const std::byte> data) -> Expected<std::size_t, NetError> override
    {
        platform::Lock lock(mutex);
        if (auto* tunnel = std::get_if<Tunnel>(&route))
            return (*tunnel)->send(data);
        return std::get<Lan>(route)->send(data);
    }

    auto receive(std::span<std::byte> buffer) -> Expected<std::size_t, NetError> override
    {
        platform::Lock lock(mutex);
        if (auto* tunnel = std::get_if<Tunnel>(&route))
            return (*tunnel)->receive(buffer);
        return std::get<Lan>(route)->receive(buffer);
    }

    auto local_endpoint() -> Expected<SocketEndpoint, NetError> override
    {
        platform::Lock lock(mutex);
        if (auto* tunnel = std::get_if<Tunnel>(&route))
            return (*tunnel)->localEndpoint();
        return std::get<Lan>(route)->local_endpoint();
    }

    auto peer_endpoint() -> Expected<SocketEndpoint, NetError> override
    {
        platform::Lock lock(mutex);
        if (auto* tunnel = std::get_if<Tunnel>(&route))
            return (*tunnel)->peerEndpoint();
        return std::get<Lan>(route)->peer_endpoint();
    }

    auto close() -> Expected<void, NetError> override
    {
        platform::Lock lock(mutex);
        decided = true;
        if (auto* tunnel = std::get_if<Tunnel>(&route))
            return (*tunnel)->close();
        return std::get<Lan>(route)->close();
    }

private:
    using Lan = std::unique_ptr<platform::TcpSocket>;
    using Tunnel = std::unique_ptr<TunnelStack::Stream>;

    auto routeFor(const SocketEndpoint& destination) -> Expected<void, NetError>
    {
        if (decided)
            return {};
        if (!ipv4 || !network.tunnels(destination.address))
        {
            decided = true;
            return {};
        }
        auto opened = TunnelStack::Stream::open(network.tunnel);
        if (!opened)
            return unexpected(opened.error());
        if (port != 0)
        {
            if (auto bound = opened.value()->bind(port); !bound)
                return bound;
        }
        static_cast<void>(std::get<Lan>(route)->close());
        route = std::move(opened).value();
        decided = true;
        return {};
    }

    RoutingNetworkProvider& network;
    const bool ipv4;
    mutable platform::Mutex mutex;
    mutable bool decided;
    std::variant<Lan, Tunnel> route;
    std::uint16_t port = 0;
};

class RoutingNetworkProvider::TcpListener final : public platform::TcpListener, public Routed {
public:
    TcpListener(RoutingNetworkProvider& network, bool ipv4, std::unique_ptr<platform::TcpListener> lan)
        : network(network)
        , ipv4(ipv4)
        , lan(std::move(lan))
    {
    }

    ~TcpListener() override = default;

    auto owner() const noexcept -> const platform::NetworkProvider& override
    {
        return network;
    }

    auto target() const -> Target override
    {
        return Target{lan.get()};
    }

    auto bind(const SocketEndpoint& endpoint) -> Expected<void, NetError> override
    {
        return lan->bind(endpoint);
    }

    auto listen(int backlog) -> Expected<void, NetError> override
    {
        return lan->listen(backlog);
    }

    auto accept() -> Expected<std::unique_ptr<platform::TcpSocket>, NetError> override
    {
        auto accepted = lan->accept();
        if (!accepted)
            return unexpected(accepted.error());
        return std::unique_ptr<platform::TcpSocket>{
            std::make_unique<TcpSocket>(network, ipv4, std::move(accepted).value(), true)};
    }

    auto local_endpoint() -> Expected<SocketEndpoint, NetError> override
    {
        return lan->local_endpoint();
    }

    auto close() -> Expected<void, NetError> override
    {
        return lan->close();
    }

private:
    RoutingNetworkProvider& network;
    const bool ipv4;
    std::unique_ptr<platform::TcpListener> lan;
};

class RoutingNetworkProvider::Poller final : public platform::Poller, private TunnelStack::Watcher {
public:
    static constexpr std::size_t maximumRequests = 8192;
    static constexpr std::chrono::hours longestWait{24};

    Poller(RoutingNetworkProvider& network, std::unique_ptr<platform::Poller> lan)
        : network(network)
        , lan(std::move(lan))
    {
    }

    ~Poller() override = default;

    auto owner() const noexcept -> const platform::NetworkProvider& override
    {
        return network;
    }

    auto wake() -> Expected<void, NetError> override
    {
        external.store(true, std::memory_order_release);
        return lan->wake();
    }

    auto wait(std::span<const platform::PollRequest> requests, std::chrono::nanoseconds timeout,
              const platform::StopToken& stop) -> Expected<platform::PollWaitResult, NetError> override
    {
        if (requests.size() > maximumRequests)
            return unexpected(NetError::invalid_argument);

        std::vector<platform::PollRequest> lanRequests;
        std::vector<Watched> watched;
        lanRequests.reserve(requests.size());
        for (std::size_t index = 0; index < requests.size(); ++index)
        {
            const auto& request = requests[index];
            if (request.pollable == nullptr || &request.pollable->owner() != &network)
                return unexpected(NetError::wrong_provider);
            const auto* routed = dynamic_cast<const Routed*>(request.pollable);
            if (routed == nullptr)
                return unexpected(NetError::wrong_provider);
            const auto target = routed->target();
            if (const auto* inner = std::get_if<const platform::Pollable*>(&target))
                lanRequests.push_back(platform::PollRequest{*inner, index, request.interest});
            else
                watched.push_back(Watched{index, std::get<const TunnelStack::Endpoint*>(target), request.interest});
        }

        const auto deadline = reading() + std::chrono::ceil<std::chrono::microseconds>(
            std::clamp<std::chrono::nanoseconds>(timeout, std::chrono::nanoseconds::zero(), longestWait));
        std::optional<TunnelStack::Watch> watch;
        if (!watched.empty())
            watch.emplace(*network.tunnel, static_cast<TunnelStack::Watcher&>(*this));

        std::vector<PollEvent> ready(requests.size(), PollEvent::none);
        while (true)
        {
            if (stop.stop_requested())
                return stopped();
            armed.store(true, std::memory_order_release);
            std::ranges::fill(ready, PollEvent::none);
            auto early = collect(watched, ready);
            if (!early)
                return unexpected(early.error());
            const auto now = reading();
            const auto remaining = early.value() || now >= deadline ? std::chrono::nanoseconds::zero()
                                                                    : std::chrono::nanoseconds{deadline - now};
            auto waited = lan->wait(lanRequests, remaining, stop);
            armed.store(false, std::memory_order_release);
            if (!waited)
                return unexpected(waited.error());
            if (waited->stop_requested)
                return stopped();
            if (auto late = collect(watched, ready); !late)
                return unexpected(late.error());
            for (const auto& readiness : waited->ready)
                ready[readiness.tag] = ready[readiness.tag] | readiness.ready;

            platform::PollWaitResult result;
            result.woke = external.exchange(false, std::memory_order_acq_rel);
            for (std::size_t index = 0; index < requests.size(); ++index)
            {
                if (ready[index] != PollEvent::none)
                    result.ready.push_back(platform::PollReadiness{requests[index].tag, ready[index]});
            }
            if (result.woke || !result.ready.empty() || reading() >= deadline)
                return result;
        }
    }

private:
    struct Watched {
        std::size_t index;
        const TunnelStack::Endpoint* endpoint;
        PollEvent interest;
    };

    void tunnelChanged() noexcept override
    {
        if (armed.exchange(false, std::memory_order_acq_rel))
            static_cast<void>(lan->wake());
    }

    static auto collect(const std::vector<Watched>& watched, std::vector<PollEvent>& ready)
        -> Expected<bool, NetError>
    {
        bool any = false;
        for (const auto& entry : watched)
        {
            auto readiness = entry.endpoint->readiness(entry.interest);
            if (!readiness)
                return unexpected(readiness.error());
            ready[entry.index] = ready[entry.index] | readiness.value();
            any = any || readiness.value() != PollEvent::none;
        }
        return any;
    }

    static auto stopped() -> platform::PollWaitResult
    {
        platform::PollWaitResult result;
        result.stop_requested = true;
        return result;
    }

    auto reading() const noexcept -> std::chrono::microseconds
    {
        return std::chrono::microseconds{static_cast<std::int64_t>(network.clock.now_microseconds())};
    }

    RoutingNetworkProvider& network;
    std::unique_ptr<platform::Poller> lan;
    std::atomic<bool> external{false};
    std::atomic<bool> armed{false};
};

auto RoutingNetworkProvider::Prefix::parse(std::string_view text) -> std::optional<Prefix>
{
    const auto slash = text.find('/');
    const auto address = platform::parse_ip_address(text.substr(0, slash));
    if (!address || !address->is_ipv4())
        return std::nullopt;
    unsigned length = 32;
    if (slash != std::string_view::npos)
    {
        const auto digits = text.substr(slash + 1);
        const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), length);
        if (error != std::errc{} || end != digits.data() + digits.size() || length > 32)
            return std::nullopt;
    }
    const auto bits = static_cast<std::uint8_t>(length);
    return Prefix{address->ipv4_number() & mask(bits), bits};
}

auto RoutingNetworkProvider::Prefix::host(const IpAddress& address) -> std::optional<Prefix>
{
    if (!address.is_ipv4() || address.is_unspecified() || address.is_loopback())
        return std::nullopt;
    return Prefix{address.ipv4_number(), 32};
}

RoutingNetworkProvider::Prefix::Prefix(std::uint32_t network, std::uint8_t length) noexcept
    : network(network)
    , length(length)
{
}

auto RoutingNetworkProvider::Prefix::mask(std::uint8_t length) noexcept -> std::uint32_t
{
    return length == 0 ? 0u : ~std::uint32_t{0} << (32 - length);
}

auto RoutingNetworkProvider::Prefix::contains(const IpAddress& address) const noexcept -> bool
{
    return address.is_ipv4() && (address.ipv4_number() & mask(length)) == network;
}

auto RoutingNetworkProvider::Prefix::singleHost() const -> std::optional<IpAddress>
{
    if (length != 32)
        return std::nullopt;
    const auto address = IpAddress::ipv4({static_cast<std::uint8_t>(network >> 24),
                                          static_cast<std::uint8_t>(network >> 16),
                                          static_cast<std::uint8_t>(network >> 8),
                                          static_cast<std::uint8_t>(network)});
    if (address.is_unspecified() || address.is_loopback())
        return std::nullopt;
    return address;
}

RoutingNetworkProvider::RoutingNetworkProvider(platform::NetworkProvider& lan, platform::ThreadingProvider& threading,
                                               const platform::ClockProvider& clock,
                                               std::shared_ptr<TunnelStack> tunnel, std::vector<Prefix> tunnelled)
    : lan(lan)
    , threading(threading)
    , clock(clock)
    , tunnel(std::move(tunnel))
    , tunnelled(std::move(tunnelled))
    , everythingTunnelled(false)
{
}

RoutingNetworkProvider::RoutingNetworkProvider(Private, platform::NetworkProvider& lan,
                                               platform::ThreadingProvider& threading,
                                               const platform::ClockProvider& clock,
                                               std::shared_ptr<TunnelStack> tunnel)
    : lan(lan)
    , threading(threading)
    , clock(clock)
    , tunnel(std::move(tunnel))
    , everythingTunnelled(true)
{
    if (auto everything = Prefix::parse("0.0.0.0/0"))
        tunnelled.push_back(*everything);
}

RoutingNetworkProvider::~RoutingNetworkProvider() = default;

auto RoutingNetworkProvider::tunnelOnly(platform::NetworkProvider& lan, platform::ThreadingProvider& threading,
                                        const platform::ClockProvider& clock, std::shared_ptr<TunnelStack> tunnel)
    -> std::shared_ptr<RoutingNetworkProvider>
{
    return std::make_shared<RoutingNetworkProvider>(Private{}, lan, threading, clock, std::move(tunnel));
}

auto RoutingNetworkProvider::capabilities() const noexcept -> platform::NetworkCapabilities
{
    auto available = lan.capabilities();
    available.broadcast = false;
    return available;
}

auto RoutingNetworkProvider::tunnels(const IpAddress& destination) const noexcept -> bool
{
    if (tunnel == nullptr || !destination.is_ipv4() || destination.is_unspecified() || destination.is_loopback())
        return false;
    return std::ranges::any_of(tunnelled, [&destination](const Prefix& prefix) {
        return prefix.contains(destination);
    });
}

auto RoutingNetworkProvider::create_udp_socket(const IpAddress& family_of)
    -> Expected<std::unique_ptr<platform::UdpSocket>, NetError>
{
    if (everythingTunnelled)
    {
        if (!family_of.is_ipv4() || tunnel == nullptr)
            return unexpected(NetError::not_supported);
        auto opened = TunnelStack::Datagram::open(tunnel);
        if (!opened)
            return unexpected(opened.error());
        return std::unique_ptr<platform::UdpSocket>{std::make_unique<UdpSocket>(*this, std::move(opened).value())};
    }
    auto created = lan.sockets().create_udp_socket(family_of);
    if (!created)
        return unexpected(created.error());
    return std::unique_ptr<platform::UdpSocket>{
        std::make_unique<UdpSocket>(*this, family_of.is_ipv4(), std::move(created).value())};
}

auto RoutingNetworkProvider::create_tcp_socket(const IpAddress& family_of)
    -> Expected<std::unique_ptr<platform::TcpSocket>, NetError>
{
    if (everythingTunnelled)
    {
        if (!family_of.is_ipv4() || tunnel == nullptr)
            return unexpected(NetError::not_supported);
        auto opened = TunnelStack::Stream::open(tunnel);
        if (!opened)
            return unexpected(opened.error());
        return std::unique_ptr<platform::TcpSocket>{std::make_unique<TcpSocket>(*this, std::move(opened).value())};
    }
    auto created = lan.sockets().create_tcp_socket(family_of);
    if (!created)
        return unexpected(created.error());
    return std::unique_ptr<platform::TcpSocket>{
        std::make_unique<TcpSocket>(*this, family_of.is_ipv4(), std::move(created).value(), false)};
}

auto RoutingNetworkProvider::create_tcp_listener(const IpAddress& family_of)
    -> Expected<std::unique_ptr<platform::TcpListener>, NetError>
{
    if (everythingTunnelled)
        return unexpected(NetError::not_supported);
    auto created = lan.sockets().create_tcp_listener(family_of);
    if (!created)
        return unexpected(created.error());
    return std::unique_ptr<platform::TcpListener>{
        std::make_unique<TcpListener>(*this, family_of.is_ipv4(), std::move(created).value())};
}

auto RoutingNetworkProvider::create_poller() -> Expected<std::unique_ptr<platform::Poller>, NetError>
{
    auto created = lan.pollers().create_poller();
    if (!created)
        return unexpected(created.error());
    return std::unique_ptr<platform::Poller>{std::make_unique<Poller>(*this, std::move(created).value())};
}

}
