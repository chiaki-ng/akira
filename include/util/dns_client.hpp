#ifndef AKIRA_DNS_CLIENT_HPP
#define AKIRA_DNS_CLIENT_HPP

#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "util/ipv4_address.hpp"

namespace akira::net {

class DnsClient {
public:
    struct Record {
        Ipv4Address address;
        std::chrono::seconds ttl;
    };

    enum class Step {
        EncodeQuestion,
        Socket,
        SocketTimeout,
        SendQuery,
        ReceiveAnswer,
        UnexpectedSource,
        Malformed,
        Truncated,
        NoSuchName,
        ServerFailure,
    };

    struct Failure {
        Step step;
        int osErrno = 0;
    };

    explicit DnsClient(std::chrono::milliseconds timeout) : m_timeout(timeout) {}

    [[nodiscard]] std::expected<std::vector<Record>, Failure> queryA(Ipv4Address server, std::string_view name) const;

    static std::optional<std::vector<std::uint8_t>> encodeQuery(std::uint16_t id, std::string_view name);
    [[nodiscard]] static std::expected<std::vector<Record>, Step> parseResponse(std::span<const std::uint8_t> packet,
        std::uint16_t expectedId);
    static const char* stepName(Step step);

private:
    static constexpr std::uint16_t TypeA = 1;
    static constexpr std::uint16_t ClassIn = 1;
    static constexpr std::size_t MaxPacket = 1232;

    static std::uint16_t nextId();
    static bool skipName(std::span<const std::uint8_t> packet, std::size_t& offset);
    static std::uint16_t read16(std::span<const std::uint8_t> packet, std::size_t offset);
    static std::uint32_t read32(std::span<const std::uint8_t> packet, std::size_t offset);

    std::chrono::milliseconds m_timeout;
};

} // namespace akira::net

#endif // AKIRA_DNS_CLIENT_HPP
