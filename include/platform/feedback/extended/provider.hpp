#pragma once

#include <cstdint>

#include "platform/feedback/provider.hpp"

namespace akira::play {
class HapticManager;
}

namespace akira::platform::extended
{

class ExtendedPadProvider;

class ExtendedFeedbackProvider final : public FeedbackProvider
{
public:
    ExtendedFeedbackProvider(ExtendedPadProvider& pads, play::HapticManager& haptics);

    void apply(const libchiaking::controllers::ControllerFeedback& feedback) override;

private:
    void applyTriggerEffects(const libchiaking::controllers::TriggerEffects& effects);
    void applyIntensity();

    ExtendedPadProvider& m_pads;
    play::HapticManager& m_haptics;
    std::uint8_t m_haptic_intensity = 1;
    std::uint8_t m_trigger_intensity = 1;
};

}
