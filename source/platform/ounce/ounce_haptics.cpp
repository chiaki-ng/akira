#include "platform/ounce/ounce_haptics.hpp"

namespace akira::platform
{

input::RumbleSource OunceHaptics::effectiveSource(input::RumbleSource configured) const
{
    return configured;
}

void OunceHaptics::prepare(HapticManager&)
{
}

bool OunceHaptics::play(HapticManager&, std::span<const std::int16_t>)
{
    return false;
}

}
