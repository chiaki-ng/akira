#pragma once

#include "platform/pad_haptics.hpp"

namespace akira::platform
{

class OunceHaptics final : public PadHaptics
{
public:
    input::RumbleSource effectiveSource(input::RumbleSource configured) const override;
    void prepare(HapticManager& haptics) override;
    bool play(HapticManager& haptics, std::span<const std::int16_t> stereo) override;
};

}
