#pragma once

#include "platform/haptics/provider.hpp"

namespace akira::platform::hid
{

class HidHapticsProvider final : public HapticsProvider
{
public:
    play::RumbleSource effectiveSource(play::RumbleSource configured) const override;
    void prepare(play::HapticManager& haptics) override;
    bool play(play::HapticManager& haptics, std::span<const std::int16_t> stereo) override;
};

}
