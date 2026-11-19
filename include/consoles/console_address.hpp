#pragma once

#include <optional>
#include <string>

namespace akira::consoles {

class ConsoleAddress {
public:
    static std::optional<ConsoleAddress> parse(std::string text);

    const std::string& text() const { return value; }

    friend bool operator==(const ConsoleAddress&, const ConsoleAddress&) = default;

private:
    explicit ConsoleAddress(std::string text);

    static bool isIpv4(const std::string& text);
    static bool isFqdn(const std::string& text);

    std::string value;
};

}
