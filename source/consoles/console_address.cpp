#include "consoles/console_address.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <regex>
#include <system_error>
#include <utility>

namespace akira::consoles {

ConsoleAddress::ConsoleAddress(std::string text)
    : value(std::move(text))
{
}

std::optional<ConsoleAddress> ConsoleAddress::parse(std::string text)
{
    if (!isIpv4(text) && !isFqdn(text))
        return std::nullopt;
    return ConsoleAddress(std::move(text));
}

bool ConsoleAddress::isIpv4(const std::string& text)
{
    static const std::regex ipv4Regex(R"(^(\d+)\.(\d+)\.(\d+)\.(\d+)$)");
    std::smatch match;
    if (!std::regex_match(text, match, ipv4Regex))
        return false;
    for (int i = 1; i <= 4; i++) {
        if (!isOctet(std::string_view(match[i].first, match[i].second)))
            return false;
    }
    return true;
}

bool ConsoleAddress::isOctet(std::string_view digits)
{
    unsigned int value = 0;
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    return error == std::errc{} && end == digits.data() + digits.size() && value <= 255;
}

bool ConsoleAddress::isFqdn(const std::string& text)
{
    if (text.empty() || text.length() > 253)
        return false;
    if (text.front() == '.' || text.back() == '.')
        return false;
    if (text.front() == '-' || text.back() == '-')
        return false;
    // TLDs must contain at least one letter - reject pure numeric strings like "192.168.50.266"
    if (!std::ranges::any_of(text, [](unsigned char c) { return std::isalpha(c); }))
        return false;
    static const std::regex fqdnRegex(R"(^[A-Za-z0-9-]+(\.[A-Za-z0-9-]+)+$)");
    return std::regex_match(text, fqdnRegex);
}

}
