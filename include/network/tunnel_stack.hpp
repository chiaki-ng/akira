#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <span>
#include <vector>

#include <libchiaking/platform/platform.hpp>

#include <lwip/netif.h>
#include <lwip/tcp.h>
#include <lwip/udp.h>

extern "C" {
#include <wireguard.h>
}

namespace akira::net {

class TunnelStack : public std::enable_shared_from_this<TunnelStack> {
    struct Private {
        explicit Private() = default;
    };

public:
    using IpAddress = libchiaking::platform::IpAddress;
    using NetError = libchiaking::platform::NetError;
    using PollEvent = libchiaking::platform::PollEvent;
    using SocketEndpoint = libchiaking::platform::SocketEndpoint;
    template <class Value>
    using Result = libchiaking::Expected<Value, NetError>;

    class Watcher {
    public:
        virtual void tunnelChanged() noexcept = 0;

    protected:
        Watcher() = default;
        ~Watcher() = default;
        Watcher(const Watcher&) = delete;
        Watcher& operator=(const Watcher&) = delete;
    };

    class Watch {
    public:
        Watch(TunnelStack& stack, Watcher& watcher);
        ~Watch();

        Watch(const Watch&) = delete;
        Watch& operator=(const Watch&) = delete;

    private:
        TunnelStack& stack;
        Watcher& watcher;
    };

    class Endpoint {
    public:
        virtual auto readiness(PollEvent interest) const -> Result<PollEvent> = 0;

    protected:
        Endpoint() = default;
        ~Endpoint() = default;
        Endpoint(const Endpoint&) = delete;
        Endpoint& operator=(const Endpoint&) = delete;

    private:
        friend class TunnelStack;

        virtual void detach() noexcept = 0;
    };

    class Datagram final : public Endpoint {
    public:
        static constexpr std::size_t defaultReceiveBuffer = 512 * 1024;
        static constexpr std::size_t maximumReceiveBuffer = 8 * 1024 * 1024;

        static auto open(std::shared_ptr<TunnelStack> stack) -> Result<std::unique_ptr<Datagram>>;
        ~Datagram();

        auto bind(std::uint16_t port) -> Result<void>;
        auto connect(const SocketEndpoint& peer) -> Result<void>;
        auto sendTo(std::span<const std::byte> datagram, const SocketEndpoint& destination) -> Result<std::size_t>;
        auto send(std::span<const std::byte> datagram) -> Result<std::size_t>;
        auto receive(std::span<std::byte> buffer) -> Result<libchiaking::platform::UdpReceiveResult>;
        auto setDontFragment(bool enabled) -> Result<void>;
        auto setReceiveBuffer(std::size_t bytes) -> Result<void>;
        auto receiveBuffer() const -> Result<std::size_t>;
        auto setTtl(std::uint8_t ttl) -> Result<void>;
        auto localEndpoint() const -> Result<SocketEndpoint>;
        auto close() -> Result<void>;

        auto readiness(PollEvent interest) const -> Result<PollEvent> override;

    private:
        enum class Phase : std::uint8_t {
            open,
            detached,
            closed
        };

        struct Received {
            std::vector<std::byte> payload;
            SocketEndpoint source;
        };

        Datagram(std::shared_ptr<TunnelStack> stack, udp_pcb* pcb);

        void detach() noexcept override;
        auto usable() const -> Result<void>;
        auto transmit(std::span<const std::byte> datagram, const ip_addr_t* destination, std::uint16_t port)
            -> Result<std::size_t>;
        void release() noexcept;

        static void onReceive(void* argument, udp_pcb* pcb, pbuf* packet, const ip_addr_t* address,
                              std::uint16_t port);

        std::shared_ptr<TunnelStack> stack;
        udp_pcb* pcb;
        Phase phase = Phase::open;
        bool dontFragment = false;
        bool connected = false;
        std::size_t capacity = defaultReceiveBuffer;
        std::size_t queuedBytes = 0;
        std::deque<Received> queue;
    };

    class Stream final : public Endpoint {
    public:
        static auto open(std::shared_ptr<TunnelStack> stack) -> Result<std::unique_ptr<Stream>>;
        ~Stream();

        auto bind(std::uint16_t port) -> Result<void>;
        auto connect(const SocketEndpoint& peer) -> Result<void>;
        auto completeConnect() -> Result<void>;
        auto send(std::span<const std::byte> data) -> Result<std::size_t>;
        auto receive(std::span<std::byte> buffer) -> Result<std::size_t>;
        auto localEndpoint() const -> Result<SocketEndpoint>;
        auto peerEndpoint() const -> Result<SocketEndpoint>;
        auto close() -> Result<void>;

        auto readiness(PollEvent interest) const -> Result<PollEvent> override;

    private:
        enum class Phase : std::uint8_t {
            idle,
            connecting,
            connected,
            peerClosed,
            failed,
            closed
        };

        Stream(std::shared_ptr<TunnelStack> stack, tcp_pcb* pcb);

        void detach() noexcept override;
        void fail(NetError error) noexcept;
        void release() noexcept;

        static auto onConnected(void* argument, tcp_pcb* pcb, err_t error) -> err_t;
        static auto onReceive(void* argument, tcp_pcb* pcb, pbuf* packet, err_t error) -> err_t;
        static auto onSent(void* argument, tcp_pcb* pcb, std::uint16_t length) -> err_t;
        static void onError(void* argument, err_t error);

        std::shared_ptr<TunnelStack> stack;
        tcp_pcb* pcb;
        Phase phase = Phase::idle;
        NetError failure = NetError::closed;
        std::deque<std::byte> inbound;
    };

    static auto start(WgTunnel& tunnel, IpAddress address, libchiaking::platform::ThreadingProvider& threading,
                      const libchiaking::platform::ClockProvider& clock) -> Result<std::shared_ptr<TunnelStack>>;

    TunnelStack(Private, WgTunnel& tunnel, IpAddress address, libchiaking::platform::ThreadingProvider& threading,
                const libchiaking::platform::ClockProvider& clock);
    ~TunnelStack();

    TunnelStack(const TunnelStack&) = delete;
    TunnelStack& operator=(const TunnelStack&) = delete;

    void stop() noexcept;
    auto address() const noexcept -> IpAddress { return localAddress; }

private:
    struct Ingress {
        WgRecvSlot* slot;
        const void* data;
        std::size_t length;
    };

    static constexpr std::size_t ingressBatch = 16;
    static constexpr std::size_t largestDatagram = 65507;
    static constexpr std::size_t datagramHeaders = 28;
    static constexpr std::size_t largestSegmentWrite = 0xffff;
    static constexpr std::uint32_t longestIdleMilliseconds = 250;

    static auto onTunnelReceive(void* user, WgRecvSlot* slot, const void* data, std::size_t length) -> int;
    static auto lwipAddress(const IpAddress& address) noexcept -> ip_addr_t;
    static auto endpointOf(const ip_addr_t& address, std::uint16_t port) noexcept -> SocketEndpoint;
    static auto netError(err_t error) noexcept -> NetError;

    auto attach() -> Result<void>;
    void run(const libchiaking::platform::StopToken& stop);
    auto admit(WgRecvSlot* slot, const void* data, std::size_t length) -> int;
    void poke() noexcept;
    void changed() noexcept;
    void adopt(Endpoint& endpoint);
    void forget(Endpoint& endpoint) noexcept;
    auto running() const noexcept -> bool { return tunnel != nullptr; }
    auto mtu() const noexcept -> std::uint16_t { return tunnelInterface.mtu; }

    WgTunnel* tunnel;
    const IpAddress localAddress;
    libchiaking::platform::ThreadingProvider& threading;
    const libchiaking::platform::ClockProvider& clock;

    mutable libchiaking::platform::Mutex lwip;
    netif tunnelInterface{};
    bool interfaceAdded = false;
    std::vector<Endpoint*> endpoints;

    libchiaking::platform::Mutex ingressMutex;
    libchiaking::platform::ConditionVariable ingressReady;
    std::deque<Ingress> ingress;
    bool accepting = true;
    bool poked = false;

    libchiaking::platform::Mutex watchersMutex;
    std::vector<Watcher*> watchers;

    libchiaking::platform::Thread loop;
};

}
