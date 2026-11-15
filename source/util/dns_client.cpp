#include "util/dns_client.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <ranges>

#include "util/net_wrappers.hpp"

namespace akira::net {

std::uint16_t DnsClient::nextId()
{
    static std::atomic<std::uint16_t> id{0x5a00};
    return id.fetch_add(1, std::memory_order_relaxed);
}

std::uint16_t DnsClient::read16(std::span<const std::uint8_t> packet, std::size_t offset)
{
    return static_cast<std::uint16_t>((packet[offset] << 8) | packet[offset + 1]);
}

std::uint32_t DnsClient::read32(std::span<const std::uint8_t> packet, std::size_t offset)
{
    return (static_cast<std::uint32_t>(read16(packet, offset)) << 16) | read16(packet, offset + 2);
}

std::optional<std::vector<std::uint8_t>> DnsClient::encodeQuery(std::uint16_t id, std::string_view name)
{
    if (name.ends_with('.'))
        name.remove_suffix(1);
    if (name.empty() || name.size() > 253)
        return std::nullopt;

    std::vector<std::uint8_t> query{
        static_cast<std::uint8_t>(id >> 8), static_cast<std::uint8_t>(id), 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    for (const auto label : std::views::split(name, '.')) {
        if (label.empty() || label.size() > 63)
            return std::nullopt;
        query.push_back(static_cast<std::uint8_t>(label.size()));
        query.insert(query.end(), label.begin(), label.end());
    }
    query.insert(query.end(), {0x00, 0x00, TypeA, 0x00, ClassIn});
    return query;
}

bool DnsClient::skipName(std::span<const std::uint8_t> packet, std::size_t& offset)
{
    for (std::size_t labels = 0; labels < 128; ++labels) {
        if (offset >= packet.size())
            return false;
        const std::uint8_t label = packet[offset++];
        if ((label & 0xc0) == 0xc0) {
            if (offset >= packet.size())
                return false;
            ++offset;
            return true;
        }
        if ((label & 0xc0) != 0)
            return false;
        if (label == 0)
            return true;
        if (offset + label > packet.size())
            return false;
        offset += label;
    }
    return false;
}

std::expected<std::vector<DnsClient::Record>, DnsClient::Step> DnsClient::parseResponse(std::span<const std::uint8_t> packet,
    std::uint16_t expectedId)
{
    if (packet.size() < 12)
        return std::unexpected(Step::Malformed);
    const std::uint16_t flags = read16(packet, 2);
    if (read16(packet, 0) != expectedId || (flags & 0x8000) == 0)
        return std::unexpected(Step::Malformed);
    if ((flags & 0x0200) != 0)
        return std::unexpected(Step::Truncated);
    if ((flags & 0x000f) == 3)
        return std::unexpected(Step::NoSuchName);
    if ((flags & 0x000f) != 0)
        return std::unexpected(Step::ServerFailure);

    const std::uint16_t questions = read16(packet, 4);
    const std::uint16_t answers = read16(packet, 6);
    if (questions != 1 || answers > 64)
        return std::unexpected(Step::Malformed);

    std::size_t offset = 12;
    if (!skipName(packet, offset) || offset + 4 > packet.size())
        return std::unexpected(Step::Malformed);
    offset += 4;

    std::vector<Record> records;
    for (std::uint16_t i = 0; i < answers; ++i) {
        if (!skipName(packet, offset) || offset + 10 > packet.size())
            return std::unexpected(Step::Malformed);
        const std::uint16_t type = read16(packet, offset);
        const std::uint16_t klass = read16(packet, offset + 2);
        const std::uint32_t ttl = read32(packet, offset + 4);
        const std::uint16_t length = read16(packet, offset + 8);
        offset += 10;
        if (offset + length > packet.size())
            return std::unexpected(Step::Malformed);
        if (type == TypeA && klass == ClassIn && length == 4) {
            std::uint32_t networkOrder = 0;
            std::copy_n(packet.begin() + static_cast<std::ptrdiff_t>(offset), 4, reinterpret_cast<std::uint8_t*>(&networkOrder));
            records.push_back({Ipv4Address::fromNetworkOrder(networkOrder), std::chrono::seconds(ttl)});
        }
        offset += length;
    }
    return records;
}

std::expected<std::vector<DnsClient::Record>, DnsClient::Failure> DnsClient::queryA(Ipv4Address server, std::string_view name) const
{
    const std::uint16_t id = nextId();
    const auto query = encodeQuery(id, name);
    if (!query)
        return std::unexpected(Failure{Step::EncodeQuestion});

    SocketGuard socketFd(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (socketFd < 0)
        return std::unexpected(Failure{Step::Socket, errno});

    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(m_timeout).count();
    const timeval timeout{.tv_sec = static_cast<time_t>(micros / 1'000'000), .tv_usec = static_cast<suseconds_t>(micros % 1'000'000)};
    if (setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        setsockopt(socketFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0)
        return std::unexpected(Failure{Step::SocketTimeout, errno});

    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(53);
    destination.sin_addr.s_addr = server.networkOrder();
    const ssize_t sent = sendto(socketFd, query->data(), query->size(), 0, reinterpret_cast<const sockaddr*>(&destination),
        sizeof(destination));
    if (sent != static_cast<ssize_t>(query->size()))
        return std::unexpected(Failure{Step::SendQuery, sent < 0 ? errno : 0});

    std::vector<std::uint8_t> response(MaxPacket);
    for (;;) {
        sockaddr_in source{};
        socklen_t sourceLength = sizeof(source);
        const ssize_t received = recvfrom(socketFd, response.data(), response.size(), 0, reinterpret_cast<sockaddr*>(&source),
            &sourceLength);
        if (received <= 0)
            return std::unexpected(Failure{Step::ReceiveAnswer, received < 0 ? errno : 0});
        if (sourceLength < sizeof(sockaddr_in) || source.sin_family != AF_INET || source.sin_addr.s_addr != server.networkOrder() ||
            source.sin_port != htons(53))
            return std::unexpected(Failure{Step::UnexpectedSource});
        auto parsed = parseResponse(std::span(response).first(static_cast<std::size_t>(received)), id);
        if (!parsed && parsed.error() == Step::Malformed && read16(response, 0) != id)
            continue;
        if (!parsed)
            return std::unexpected(Failure{parsed.error()});
        return std::move(parsed).value();
    }
}

const char* DnsClient::stepName(Step step)
{
    switch (step) {
    case Step::EncodeQuestion: return "encode_question";
    case Step::Socket: return "socket";
    case Step::SocketTimeout: return "socket_timeout";
    case Step::SendQuery: return "send_query";
    case Step::ReceiveAnswer: return "receive_answer";
    case Step::UnexpectedSource: return "unexpected_answer_source";
    case Step::Malformed: return "invalid_answer";
    case Step::Truncated: return "truncated_answer";
    case Step::NoSuchName: return "no_such_name";
    case Step::ServerFailure: return "server_failure";
    }
    return "unknown";
}

} // namespace akira::net
