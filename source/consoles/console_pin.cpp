#include "consoles/console_pin.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace akira::consoles {

ConsolePin::ConsolePin(std::string text)
    : digits(std::move(text))
{
}

std::optional<ConsolePin> ConsolePin::parse(std::string text)
{
    if (!text.empty() && (text.length() != length || !std::ranges::all_of(text, ::isdigit)))
        return std::nullopt;
    return ConsolePin(std::move(text));
}

}
