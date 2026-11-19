#pragma once

#include <cstdint>
#include <span>

#include "play/rumble_profile.hpp"

class HapticManager;

namespace akira::platform
{

class HapticsProvider
{
public:
    virtual ~HapticsProvider() = default;

    virtual input::RumbleSource effectiveSource(input::RumbleSource configured) const = 0;
    virtual void prepare(HapticManager& haptics) = 0;
    virtual bool play(HapticManager& haptics, std::span<const std::int16_t> stereo) = 0;
};

}
