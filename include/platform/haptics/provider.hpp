#pragma once

#include <cstdint>
#include <span>

#include "play/rumble_profile.hpp"

namespace akira::play {
class HapticManager;
}

namespace akira::platform
{

class HapticsProvider
{
public:
    virtual ~HapticsProvider() = default;

    virtual play::RumbleSource effectiveSource(play::RumbleSource configured) const = 0;
    virtual void prepare(play::HapticManager& haptics) = 0;
    virtual bool play(play::HapticManager& haptics, std::span<const std::int16_t> stereo) = 0;
};

}
