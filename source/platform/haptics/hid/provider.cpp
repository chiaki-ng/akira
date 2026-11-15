#include "platform/haptics/hid/provider.hpp"

namespace akira::platform::hid
{

input::RumbleSource HidHapticsProvider::effectiveSource(input::RumbleSource configured) const
{
    return configured;
}

void HidHapticsProvider::prepare(HapticManager&)
{
}

bool HidHapticsProvider::play(HapticManager&, std::span<const std::int16_t>)
{
    return false;
}

}
