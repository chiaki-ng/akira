#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace akira::consoles {

class ConsolePin {
public:
    static constexpr std::size_t length = 4;

    static std::optional<ConsolePin> parse(std::string text);

    const std::string& text() const { return digits; }

private:
    explicit ConsolePin(std::string text);

    std::string digits;
};

}
