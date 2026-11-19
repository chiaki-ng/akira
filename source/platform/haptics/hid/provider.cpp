#include "platform/haptics/hid/provider.hpp"

namespace akira::platform::hid
{

play::RumbleSource HidHapticsProvider::effectiveSource(play::RumbleSource configured) const
{
    return configured;
}

void HidHapticsProvider::prepare(play::HapticManager&)
{
}

bool HidHapticsProvider::play(play::HapticManager&, std::span<const std::int16_t>)
{
    return false;
}

}
