#ifndef AKIRA_IPV4_ADDRESS_HPP
#define AKIRA_IPV4_ADDRESS_HPP

#include <arpa/inet.h>
#include <sys/socket.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace akira::net {

class Ipv4Address {
public:
    static constexpr Ipv4Address fromNetworkOrder(std::uint32_t value) { return Ipv4Address(value); }

    static std::optional<Ipv4Address> parse(std::string_view text)
    {
        if (text.empty() || text.size() >= INET_ADDRSTRLEN)
            return std::nullopt;
        const std::string terminated(text);
        in_addr address{};
        if (inet_pton(AF_INET, terminated.c_str(), &address) != 1)
            return std::nullopt;
        return Ipv4Address(address.s_addr);
    }

    std::uint32_t networkOrder() const { return m_value; }
    bool isUnspecified() const { return m_value == 0; }

    std::string toString() const
    {
        in_addr address{};
        address.s_addr = m_value;
        char text[INET_ADDRSTRLEN]{};
        return inet_ntop(AF_INET, &address, text, sizeof(text)) ? text : "?";
    }

    friend bool operator==(Ipv4Address, Ipv4Address) = default;

private:
    explicit constexpr Ipv4Address(std::uint32_t value) : m_value(value) {}

    std::uint32_t m_value;
};

} // namespace akira::net

#endif // AKIRA_IPV4_ADDRESS_HPP
