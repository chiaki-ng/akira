#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <libchiaking/platform/platform.hpp>

namespace akira::net {

class TunnelStack;

class RoutingNetworkProvider final : public libchiaking::platform::NetworkProvider,
                                     private libchiaking::platform::SocketProvider,
                                     private libchiaking::platform::PollerProvider {
    struct Private {
        explicit Private() = default;
    };

public:
    using IpAddress = libchiaking::platform::IpAddress;
    using NetError = libchiaking::platform::NetError;

    class Prefix {
    public:
        static auto parse(std::string_view text) -> std::optional<Prefix>;
        static auto host(const IpAddress& address) -> std::optional<Prefix>;

        auto contains(const IpAddress& address) const noexcept -> bool;
        auto singleHost() const -> std::optional<IpAddress>;

    private:
        Prefix(std::uint32_t network, std::uint8_t length) noexcept;

        static auto mask(std::uint8_t length) noexcept -> std::uint32_t;

        std::uint32_t network;
        std::uint8_t length;
    };

    RoutingNetworkProvider(libchiaking::platform::NetworkProvider& lan,
                           libchiaking::platform::ThreadingProvider& threading,
                           const libchiaking::platform::ClockProvider& clock, std::shared_ptr<TunnelStack> tunnel,
                           std::vector<Prefix> tunnelled);
    RoutingNetworkProvider(Private, libchiaking::platform::NetworkProvider& lan,
                           libchiaking::platform::ThreadingProvider& threading,
                           const libchiaking::platform::ClockProvider& clock, std::shared_ptr<TunnelStack> tunnel);
    ~RoutingNetworkProvider() override;

    static auto tunnelOnly(libchiaking::platform::NetworkProvider& lan,
                           libchiaking::platform::ThreadingProvider& threading,
                           const libchiaking::platform::ClockProvider& clock, std::shared_ptr<TunnelStack> tunnel)
        -> std::shared_ptr<RoutingNetworkProvider>;

    RoutingNetworkProvider(const RoutingNetworkProvider&) = delete;
    RoutingNetworkProvider& operator=(const RoutingNetworkProvider&) = delete;

    auto sockets() noexcept -> libchiaking::platform::SocketProvider& override { return *this; }
    auto pollers() noexcept -> libchiaking::platform::PollerProvider& override { return *this; }
    auto dns() noexcept -> libchiaking::platform::DnsProvider& override { return lan.dns(); }
    auto interfaces() noexcept -> libchiaking::platform::InterfacesProvider& override { return lan.interfaces(); }
    auto capabilities() const noexcept -> libchiaking::platform::NetworkCapabilities override;

    auto tunnels(const IpAddress& destination) const noexcept -> bool;

private:
    class Routed;
    class UdpSocket;
    class TcpSocket;
    class TcpListener;
    class Poller;

    auto create_udp_socket(const IpAddress& family_of)
        -> libchiaking::Expected<std::unique_ptr<libchiaking::platform::UdpSocket>, NetError> override;
    auto create_tcp_socket(const IpAddress& family_of)
        -> libchiaking::Expected<std::unique_ptr<libchiaking::platform::TcpSocket>, NetError> override;
    auto create_tcp_listener(const IpAddress& family_of)
        -> libchiaking::Expected<std::unique_ptr<libchiaking::platform::TcpListener>, NetError> override;
    auto create_poller()
        -> libchiaking::Expected<std::unique_ptr<libchiaking::platform::Poller>, NetError> override;

    libchiaking::platform::NetworkProvider& lan;
    libchiaking::platform::ThreadingProvider& threading;
    const libchiaking::platform::ClockProvider& clock;
    std::shared_ptr<TunnelStack> tunnel;
    std::vector<Prefix> tunnelled;
    const bool everythingTunnelled;
};

}
