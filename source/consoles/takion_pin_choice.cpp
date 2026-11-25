#include "consoles/takion_pin_choice.hpp"

#include <algorithm>

namespace akira::consoles {

TakionPinChoice::TakionPinChoice(std::span<const std::uint32_t> versions, std::optional<std::uint32_t> pinned,
                                 int configured)
    : pinnable(versions.begin(), versions.end())
    , pinned(pinned)
    , configured(configured)
{
}

std::optional<std::uint32_t> TakionPinChoice::configuredVersion() const
{
    if (configured > 0)
        return static_cast<std::uint32_t>(configured);
    return std::nullopt;
}

int TakionPinChoice::selected() const
{
    const auto found = pinned ? std::ranges::find(pinnable, *pinned) : pinnable.end();
    return found == pinnable.end() ? 0 : static_cast<int>(found - pinnable.begin()) + 1;
}

bool TakionPinChoice::contains(int index) const
{
    return index >= 0 && index <= static_cast<int>(pinnable.size());
}

std::optional<std::uint32_t> TakionPinChoice::versionAt(int index) const
{
    if (index <= 0 || !contains(index))
        return std::nullopt;
    return pinnable[static_cast<std::size_t>(index - 1)];
}

}
