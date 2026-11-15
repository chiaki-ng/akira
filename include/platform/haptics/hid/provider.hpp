#pragma once

#include "platform/haptics/provider.hpp"

namespace akira::platform::hid
{

class HidHapticsProvider final : public HapticsProvider
{
public:
    input::RumbleSource effectiveSource(input::RumbleSource configured) const override;
    void prepare(HapticManager& haptics) override;
    bool play(HapticManager& haptics, std::span<const std::int16_t> stereo) override;
};

}
