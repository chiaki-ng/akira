#include "core/net/tunnel_stack.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <utility>

#include <lwip/init.h>
#include <lwip/ip.h>
#include <lwip/pbuf.h>
#include <lwip/timeouts.h>

#include <wg_netif.h>

namespace akira::net {

namespace platform = libchiaking::platform;
using libchiaking::unexpected;

TunnelStack::Watch::Watch(TunnelStack& stack, Watcher& watcher)
    : stack(stack)
    , watcher(watcher)
{
    platform::Lock lock(stack.watchersMutex);
    stack.watchers.push_back(&watcher);
}

TunnelStack::Watch::~Watch()
{
    platform::Lock lock(stack.watchersMutex);
    std::erase(stack.watchers, &watcher);
}

auto TunnelStack::start(WgTunnel& tunnel, IpAddress address, platform::ThreadingProvider& threading,
                        const platform::ClockProvider& clock) -> Result<std::shared_ptr<TunnelStack>>
{
    if (!address.is_ipv4() || address.is_unspecified())
        return unexpected(NetError::invalid_argument);
    auto stack = std::make_shared<TunnelStack>(Private{}, tunnel, address, threading, clock);
    if (auto attached = stack->attach(); !attached)
        return unexpected(attached.error());
    auto started = platform::Thread::start(threading, "lwip", platform::ThreadRole::transport,
        [raw = stack.get()](platform::StopToken stop) {
            raw->run(stop);
        });
    if (!started)
    {
        stack->stop();
        return unexpected(NetError::resource_exhausted);
    }
    stack->loop = std::move(started).value();
    wg_set_recv_callback(&tunnel, &TunnelStack::onTunnelReceive, stack.get());
    return stack;
}

TunnelStack::TunnelStack(Private, WgTunnel& tunnel, IpAddress address, platform::ThreadingProvider& threading,
                         const platform::ClockProvider& clock)
    : tunnel(&tunnel)
    , localAddress(address)
    , threading(threading)
    , clock(clock)
    , lwip(threading)
    , ingressMutex(threading)
    , ingressReady(threading, clock)
    , watchersMutex(threading)
{
}

TunnelStack::~TunnelStack()
{
    stop();
}

auto TunnelStack::attach() -> Result<void>
{
    static bool lwipInitialised = false;
    platform::Lock lock(lwip);
    if (!lwipInitialised)
    {
        lwip_init();
        lwipInitialised = true;
    }
    const ip4_addr_t ownAddress = lwipAddress(localAddress);
    ip4_addr_t netmask;
    ip4_addr_t gateway;
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gateway, 0, 0, 0, 0);
    if (netif_add(&tunnelInterface, &ownAddress, &netmask, &gateway, tunnel, wg_netif_init, ip_input) == nullptr)
        return unexpected(NetError::system_failure);
    netif_set_default(&tunnelInterface);
    netif_set_up(&tunnelInterface);
    interfaceAdded = true;
    return {};
}

void TunnelStack::stop() noexcept
{
    {
        platform::Lock lock(ingressMutex);
        if (!accepting)
            return;
        accepting = false;
    }
    WgTunnel* const stopping = tunnel;
    wg_set_recv_callback(stopping, nullptr, nullptr);
    loop = platform::Thread{};

    std::deque<Ingress> left;
    {
        platform::Lock lock(ingressMutex);
        left.swap(ingress);
    }
    for (const auto& packet : left)
        wg_recv_slot_release(stopping, packet.slot);

    {
        platform::Lock lock(lwip);
        for (auto* endpoint : endpoints)
            endpoint->detach();
        if (interfaceAdded)
        {
            netif_set_down(&tunnelInterface);
            netif_remove(&tunnelInterface);
            interfaceAdded = false;
        }
        tunnel = nullptr;
    }
    changed();
}

auto TunnelStack::onTunnelReceive(void* user, WgRecvSlot* slot, const void* data, std::size_t length) -> int
{
    return static_cast<TunnelStack*>(user)->admit(slot, data, length);
}

auto TunnelStack::admit(WgRecvSlot* slot, const void* data, std::size_t length) -> int
{
    if (slot == nullptr || data == nullptr || length == 0)
        return 0;
    platform::Lock lock(ingressMutex);
    if (!accepting)
        return 0;
    ingress.push_back(Ingress{slot, data, length});
    ingressReady.notify_one();
    return 1;
}

void TunnelStack::run(const platform::StopToken& stop)
{
    auto idle = std::chrono::milliseconds{longestIdleMilliseconds};
    std::vector<Ingress> batch;
    batch.reserve(ingressBatch);
    while (!stop.stop_requested())
    {
        bool more = false;
        batch.clear();
        {
            platform::Lock lock(ingressMutex);
            static_cast<void>(ingressReady.wait_for(lock, idle, stop, [this] {
                return !ingress.empty() || poked;
            }));
            poked = false;
            const auto count = std::min(ingress.size(), ingressBatch);
            batch.assign(ingress.begin(), ingress.begin() + static_cast<std::ptrdiff_t>(count));
            ingress.erase(ingress.begin(), ingress.begin() + static_cast<std::ptrdiff_t>(count));
            more = !ingress.empty();
        }

        platform::Lock lock(lwip);
        for (const auto& packet : batch)
        {
            wg_netif_input(&tunnelInterface, packet.data, packet.length);
            wg_recv_slot_release(tunnel, packet.slot);
        }
        sys_check_timeouts();
        const auto next = std::min<std::uint32_t>(sys_timeouts_sleeptime(), longestIdleMilliseconds);
        idle = more ? std::chrono::milliseconds{0} : std::chrono::milliseconds{next};
    }
}

void TunnelStack::poke() noexcept
{
    platform::Lock lock(ingressMutex);
    poked = true;
    ingressReady.notify_one();
}

void TunnelStack::changed() noexcept
{
    platform::Lock lock(watchersMutex);
    for (auto* watcher : watchers)
        watcher->tunnelChanged();
}

void TunnelStack::adopt(Endpoint& endpoint)
{
    endpoints.push_back(&endpoint);
}

void TunnelStack::forget(Endpoint& endpoint) noexcept
{
    std::erase(endpoints, &endpoint);
}

auto TunnelStack::lwipAddress(const IpAddress& address) noexcept -> ip_addr_t
{
    ip_addr_t converted{};
    const auto bytes = address.ipv4_bytes();
    std::memcpy(&converted.addr, bytes.data(), bytes.size());
    return converted;
}

auto TunnelStack::endpointOf(const ip_addr_t& address, std::uint16_t port) noexcept -> SocketEndpoint
{
    std::array<std::uint8_t, 4> bytes{};
    std::memcpy(bytes.data(), &address.addr, bytes.size());
    return SocketEndpoint{IpAddress::ipv4(bytes), port};
}

auto TunnelStack::netError(err_t error) noexcept -> NetError
{
    switch (error)
    {
    case ERR_MEM:
    case ERR_BUF:
        return NetError::no_buffers;
    case ERR_TIMEOUT:
        return NetError::timed_out;
    case ERR_RTE:
        return NetError::network_unreachable;
    case ERR_INPROGRESS:
    case ERR_WOULDBLOCK:
    case ERR_ALREADY:
        return NetError::would_block;
    case ERR_VAL:
    case ERR_ARG:
        return NetError::invalid_argument;
    case ERR_USE:
        return NetError::address_in_use;
    case ERR_ISCONN:
        return NetError::already_connected;
    case ERR_CONN:
        return NetError::not_connected;
    case ERR_IF:
        return NetError::network_down;
    case ERR_ABRT:
        return NetError::connection_aborted;
    case ERR_RST:
        return NetError::connection_reset;
    case ERR_CLSD:
        return NetError::closed;
    default:
        return NetError::system_failure;
    }
}

auto TunnelStack::Datagram::open(std::shared_ptr<TunnelStack> stack) -> Result<std::unique_ptr<Datagram>>
{
    auto& owner = *stack;
    platform::Lock lock(owner.lwip);
    if (!owner.running())
        return unexpected(NetError::network_down);
    auto* pcb = udp_new();
    if (pcb == nullptr)
        return unexpected(NetError::resource_exhausted);
    return std::unique_ptr<Datagram>{new Datagram(std::move(stack), pcb)};
}

TunnelStack::Datagram::Datagram(std::shared_ptr<TunnelStack> stack, udp_pcb* pcb)
    : stack(std::move(stack))
    , pcb(pcb)
{
    udp_recv(pcb, &Datagram::onReceive, this);
    this->stack->adopt(*this);
}

TunnelStack::Datagram::~Datagram()
{
    platform::Lock lock(stack->lwip);
    release();
    stack->forget(*this);
}

void TunnelStack::Datagram::release() noexcept
{
    if (pcb == nullptr)
        return;
    udp_recv(pcb, nullptr, nullptr);
    udp_remove(pcb);
    pcb = nullptr;
}

void TunnelStack::Datagram::detach() noexcept
{
    release();
    if (phase == Phase::open)
        phase = Phase::detached;
}

auto TunnelStack::Datagram::usable() const -> Result<void>
{
    switch (phase)
    {
    case Phase::open:
        return {};
    case Phase::detached:
        return unexpected(NetError::network_down);
    case Phase::closed:
        return unexpected(NetError::closed);
    }
    return unexpected(NetError::closed);
}

auto TunnelStack::Datagram::bind(std::uint16_t port) -> Result<void>
{
    platform::Lock lock(stack->lwip);
    if (auto ready = usable(); !ready)
        return ready;
    const ip_addr_t local = lwipAddress(stack->address());
    if (const err_t bound = udp_bind(pcb, &local, port); bound != ERR_OK)
        return unexpected(netError(bound));
    return {};
}

auto TunnelStack::Datagram::connect(const SocketEndpoint& peer) -> Result<void>
{
    if (!peer.address.is_ipv4())
        return unexpected(NetError::invalid_argument);
    platform::Lock lock(stack->lwip);
    if (auto ready = usable(); !ready)
        return ready;
    const ip_addr_t remote = lwipAddress(peer.address);
    if (const err_t attached = udp_connect(pcb, &remote, peer.port); attached != ERR_OK)
        return unexpected(netError(attached));
    connected = true;
    return {};
}

auto TunnelStack::Datagram::sendTo(std::span<const std::byte> datagram, const SocketEndpoint& destination)
    -> Result<std::size_t>
{
    if (!destination.address.is_ipv4())
        return unexpected(NetError::invalid_argument);
    const ip_addr_t remote = lwipAddress(destination.address);
    platform::Lock lock(stack->lwip);
    return transmit(datagram, &remote, destination.port);
}

auto TunnelStack::Datagram::send(std::span<const std::byte> datagram) -> Result<std::size_t>
{
    platform::Lock lock(stack->lwip);
    if (phase == Phase::open && !connected)
        return unexpected(NetError::not_connected);
    return transmit(datagram, nullptr, 0);
}

auto TunnelStack::Datagram::transmit(std::span<const std::byte> datagram, const ip_addr_t* destination,
                                     std::uint16_t port) -> Result<std::size_t>
{
    if (auto ready = usable(); !ready)
        return unexpected(ready.error());
    if (datagram.size() > largestDatagram)
        return unexpected(NetError::message_too_large);
    if (dontFragment && datagram.size() + datagramHeaders > stack->mtu())
        return unexpected(NetError::message_too_large);
    auto* packet = pbuf_alloc(PBUF_TRANSPORT, static_cast<std::uint16_t>(datagram.size()), PBUF_RAM);
    if (packet == nullptr)
        return unexpected(NetError::no_buffers);
    static_cast<void>(pbuf_take(packet, datagram.data(), static_cast<std::uint16_t>(datagram.size())));
    const err_t sent = destination != nullptr ? udp_sendto(pcb, packet, destination, port) : udp_send(pcb, packet);
    pbuf_free(packet);
    if (sent != ERR_OK && sent != ERR_IF)
        return unexpected(netError(sent));
    return datagram.size();
}

auto TunnelStack::Datagram::receive(std::span<std::byte> buffer) -> Result<platform::UdpReceiveResult>
{
    platform::Lock lock(stack->lwip);
    if (phase == Phase::closed)
        return unexpected(NetError::closed);
    if (queue.empty())
    {
        if (phase == Phase::detached)
            return unexpected(NetError::network_down);
        return unexpected(NetError::would_block);
    }
    Received front = std::move(queue.front());
    queue.pop_front();
    queuedBytes -= front.payload.size();
    const auto copied = std::min(buffer.size(), front.payload.size());
    std::copy_n(front.payload.begin(), copied, buffer.begin());
    platform::UdpReceiveResult result;
    result.size = copied;
    result.source = front.source;
    return result;
}

auto TunnelStack::Datagram::setDontFragment(bool enabled) -> Result<void>
{
    platform::Lock lock(stack->lwip);
    if (auto ready = usable(); !ready)
        return ready;
    dontFragment = enabled;
    return {};
}

auto TunnelStack::Datagram::setReceiveBuffer(std::size_t bytes) -> Result<void>
{
    platform::Lock lock(stack->lwip);
    if (auto ready = usable(); !ready)
        return ready;
    capacity = std::clamp(bytes, largestDatagram, maximumReceiveBuffer);
    return {};
}

auto TunnelStack::Datagram::receiveBuffer() const -> Result<std::size_t>
{
    platform::Lock lock(stack->lwip);
    if (auto ready = usable(); !ready)
        return unexpected(ready.error());
    return capacity;
}

auto TunnelStack::Datagram::setTtl(std::uint8_t ttl) -> Result<void>
{
    if (ttl == 0)
        return unexpected(NetError::invalid_argument);
    platform::Lock lock(stack->lwip);
    if (auto ready = usable(); !ready)
        return ready;
    pcb->ttl = ttl;
    return {};
}

auto TunnelStack::Datagram::localEndpoint() const -> Result<SocketEndpoint>
{
    platform::Lock lock(stack->lwip);
    if (auto ready = usable(); !ready)
        return unexpected(ready.error());
    return SocketEndpoint{stack->address(), pcb->local_port};
}

auto TunnelStack::Datagram::close() -> Result<void>
{
    platform::Lock lock(stack->lwip);
    if (phase == Phase::closed)
        return unexpected(NetError::closed);
    release();
    phase = Phase::closed;
    queue.clear();
    queuedBytes = 0;
    return {};
}

auto TunnelStack::Datagram::readiness(PollEvent interest) const -> Result<PollEvent>
{
    platform::Lock lock(stack->lwip);
    switch (phase)
    {
    case Phase::closed:
        return unexpected(NetError::system_failure);
    case Phase::detached:
        return PollEvent::readable | PollEvent::writable;
    case Phase::open:
        break;
    }
    auto ready = PollEvent::writable;
    if (!queue.empty())
        ready = ready | PollEvent::readable;
    return ready & interest;
}

void TunnelStack::Datagram::onReceive(void* argument, udp_pcb*, pbuf* packet, const ip_addr_t* address,
                                      std::uint16_t port)
{
    auto* self = static_cast<Datagram*>(argument);
    if (packet == nullptr)
        return;
    if (self == nullptr || address == nullptr || self->queuedBytes + packet->tot_len > self->capacity)
    {
        pbuf_free(packet);
        return;
    }
    Received received{std::vector<std::byte>(packet->tot_len), endpointOf(*address, port)};
    static_cast<void>(pbuf_copy_partial(packet, received.payload.data(), packet->tot_len, 0));
    pbuf_free(packet);
    self->queuedBytes += received.payload.size();
    self->queue.push_back(std::move(received));
    self->stack->changed();
}

auto TunnelStack::Stream::open(std::shared_ptr<TunnelStack> stack) -> Result<std::unique_ptr<Stream>>
{
    auto& owner = *stack;
    platform::Lock lock(owner.lwip);
    if (!owner.running())
        return unexpected(NetError::network_down);
    auto* pcb = tcp_new();
    if (pcb == nullptr)
        return unexpected(NetError::resource_exhausted);
    return std::unique_ptr<Stream>{new Stream(std::move(stack), pcb)};
}

TunnelStack::Stream::Stream(std::shared_ptr<TunnelStack> stack, tcp_pcb* pcb)
    : stack(std::move(stack))
    , pcb(pcb)
{
    tcp_arg(pcb, this);
    tcp_err(pcb, &Stream::onError);
    tcp_recv(pcb, &Stream::onReceive);
    tcp_sent(pcb, &Stream::onSent);
    tcp_nagle_disable(pcb);
    this->stack->adopt(*this);
}

TunnelStack::Stream::~Stream()
{
    platform::Lock lock(stack->lwip);
    release();
    stack->forget(*this);
}

void TunnelStack::Stream::release() noexcept
{
    if (pcb == nullptr)
        return;
    tcp_arg(pcb, nullptr);
    tcp_err(pcb, nullptr);
    tcp_recv(pcb, nullptr);
    tcp_sent(pcb, nullptr);
    if (tcp_close(pcb) != ERR_OK)
        tcp_abort(pcb);
    pcb = nullptr;
    stack->poke();
}

void TunnelStack::Stream::detach() noexcept
{
    if (pcb != nullptr)
    {
        tcp_arg(pcb, nullptr);
        tcp_err(pcb, nullptr);
        tcp_recv(pcb, nullptr);
        tcp_sent(pcb, nullptr);
        tcp_abort(pcb);
        pcb = nullptr;
    }
    if (phase != Phase::closed)
        fail(NetError::network_down);
}

void TunnelStack::Stream::fail(NetError error) noexcept
{
    phase = Phase::failed;
    failure = error;
}

auto TunnelStack::Stream::bind(std::uint16_t port) -> Result<void>
{
    platform::Lock lock(stack->lwip);
    if (phase == Phase::closed)
        return unexpected(NetError::closed);
    if (phase == Phase::failed)
        return unexpected(failure);
    if (phase != Phase::idle)
        return unexpected(NetError::invalid_argument);
    const ip_addr_t local = lwipAddress(stack->address());
    if (const err_t bound = tcp_bind(pcb, &local, port); bound != ERR_OK)
        return unexpected(netError(bound));
    return {};
}

auto TunnelStack::Stream::connect(const SocketEndpoint& peer) -> Result<void>
{
    if (!peer.address.is_ipv4())
        return unexpected(NetError::invalid_argument);
    platform::Lock lock(stack->lwip);
    switch (phase)
    {
    case Phase::closed:
        return unexpected(NetError::closed);
    case Phase::failed:
        return unexpected(failure);
    case Phase::connecting:
        return unexpected(NetError::would_block);
    case Phase::connected:
    case Phase::peerClosed:
        return {};
    case Phase::idle:
        break;
    }
    const ip_addr_t remote = lwipAddress(peer.address);
    if (const err_t started = tcp_connect(pcb, &remote, peer.port, &Stream::onConnected); started != ERR_OK)
        return unexpected(netError(started));
    phase = Phase::connecting;
    stack->poke();
    return unexpected(NetError::would_block);
}

auto TunnelStack::Stream::completeConnect() -> Result<void>
{
    platform::Lock lock(stack->lwip);
    switch (phase)
    {
    case Phase::connected:
    case Phase::peerClosed:
        return {};
    case Phase::connecting:
        return unexpected(NetError::would_block);
    case Phase::idle:
        return unexpected(NetError::not_connected);
    case Phase::failed:
        return unexpected(failure);
    case Phase::closed:
        return unexpected(NetError::closed);
    }
    return unexpected(NetError::closed);
}

auto TunnelStack::Stream::send(std::span<const std::byte> data) -> Result<std::size_t>
{
    platform::Lock lock(stack->lwip);
    switch (phase)
    {
    case Phase::idle:
        return unexpected(NetError::not_connected);
    case Phase::connecting:
        return unexpected(NetError::would_block);
    case Phase::failed:
    case Phase::closed:
        return unexpected(NetError::closed);
    case Phase::connected:
    case Phase::peerClosed:
        break;
    }
    if (pcb == nullptr)
        return unexpected(NetError::closed);
    const auto accepted = std::min<std::size_t>({data.size(), tcp_sndbuf(pcb), largestSegmentWrite});
    if (accepted == 0)
        return unexpected(NetError::would_block);
    const err_t written = tcp_write(pcb, data.data(), static_cast<std::uint16_t>(accepted), TCP_WRITE_FLAG_COPY);
    if (written == ERR_MEM)
        return unexpected(NetError::would_block);
    if (written != ERR_OK)
        return unexpected(netError(written));
    static_cast<void>(tcp_output(pcb));
    stack->poke();
    return accepted;
}

auto TunnelStack::Stream::receive(std::span<std::byte> buffer) -> Result<std::size_t>
{
    if (buffer.empty())
        return unexpected(NetError::invalid_argument);
    platform::Lock lock(stack->lwip);
    if (phase == Phase::closed)
        return unexpected(NetError::closed);
    if (!inbound.empty())
    {
        const auto copied = std::min(buffer.size(), inbound.size());
        std::copy_n(inbound.begin(), copied, buffer.begin());
        inbound.erase(inbound.begin(), inbound.begin() + static_cast<std::ptrdiff_t>(copied));
        if (pcb != nullptr)
        {
            for (auto left = copied; left > 0;)
            {
                const auto window = std::min(left, largestSegmentWrite);
                tcp_recved(pcb, static_cast<std::uint16_t>(window));
                left -= window;
            }
        }
        return copied;
    }
    switch (phase)
    {
    case Phase::idle:
        return unexpected(NetError::not_connected);
    case Phase::peerClosed:
    case Phase::failed:
        return unexpected(NetError::closed);
    default:
        return unexpected(NetError::would_block);
    }
}

auto TunnelStack::Stream::localEndpoint() const -> Result<SocketEndpoint>
{
    platform::Lock lock(stack->lwip);
    if (phase == Phase::closed)
        return unexpected(NetError::closed);
    if (pcb == nullptr)
        return unexpected(failure);
    return endpointOf(pcb->local_ip, pcb->local_port);
}

auto TunnelStack::Stream::peerEndpoint() const -> Result<SocketEndpoint>
{
    platform::Lock lock(stack->lwip);
    if (phase == Phase::closed)
        return unexpected(NetError::closed);
    if (pcb == nullptr || (phase != Phase::connected && phase != Phase::peerClosed))
        return unexpected(NetError::not_connected);
    return endpointOf(pcb->remote_ip, pcb->remote_port);
}

auto TunnelStack::Stream::close() -> Result<void>
{
    platform::Lock lock(stack->lwip);
    if (phase == Phase::closed)
        return unexpected(NetError::closed);
    release();
    phase = Phase::closed;
    inbound.clear();
    return {};
}

auto TunnelStack::Stream::readiness(PollEvent interest) const -> Result<PollEvent>
{
    platform::Lock lock(stack->lwip);
    auto ready = PollEvent::none;
    switch (phase)
    {
    case Phase::closed:
        return unexpected(NetError::system_failure);
    case Phase::failed:
        return PollEvent::readable | PollEvent::writable;
    case Phase::peerClosed:
        ready = PollEvent::readable;
        break;
    case Phase::idle:
    case Phase::connecting:
    case Phase::connected:
        break;
    }
    if (!inbound.empty())
        ready = ready | PollEvent::readable;
    if ((phase == Phase::connected || phase == Phase::peerClosed) && pcb != nullptr && tcp_sndbuf(pcb) > 0)
        ready = ready | PollEvent::writable;
    return ready & interest;
}

auto TunnelStack::Stream::onConnected(void* argument, tcp_pcb*, err_t error) -> err_t
{
    auto* self = static_cast<Stream*>(argument);
    if (self == nullptr)
        return ERR_ABRT;
    if (error != ERR_OK)
        self->fail(netError(error));
    else if (self->phase == Phase::connecting)
        self->phase = Phase::connected;
    self->stack->changed();
    return ERR_OK;
}

auto TunnelStack::Stream::onReceive(void* argument, tcp_pcb*, pbuf* packet, err_t error) -> err_t
{
    auto* self = static_cast<Stream*>(argument);
    if (self == nullptr)
    {
        if (packet != nullptr)
            pbuf_free(packet);
        return ERR_ABRT;
    }
    if (packet == nullptr || error != ERR_OK)
    {
        if (packet != nullptr)
            pbuf_free(packet);
        if (self->phase == Phase::connected)
            self->phase = Phase::peerClosed;
        self->stack->changed();
        return ERR_OK;
    }
    for (const pbuf* part = packet; part != nullptr; part = part->next)
    {
        const auto* bytes = static_cast<const std::byte*>(part->payload);
        self->inbound.insert(self->inbound.end(), bytes, bytes + part->len);
    }
    pbuf_free(packet);
    self->stack->changed();
    return ERR_OK;
}

auto TunnelStack::Stream::onSent(void* argument, tcp_pcb*, std::uint16_t) -> err_t
{
    if (auto* self = static_cast<Stream*>(argument))
        self->stack->changed();
    return ERR_OK;
}

void TunnelStack::Stream::onError(void* argument, err_t error)
{
    auto* self = static_cast<Stream*>(argument);
    if (self == nullptr)
        return;
    self->pcb = nullptr;
    const bool refused = self->phase == Phase::connecting && error == ERR_RST;
    self->fail(refused ? NetError::connection_refused : netError(error));
    self->stack->changed();
}

}
