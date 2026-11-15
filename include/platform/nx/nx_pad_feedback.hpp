#pragma once

#include <cstdint>

#include "platform/pad_feedback.hpp"

class HapticManager;

namespace akira::platform
{

class NxPadSource;

class NxPadFeedback final : public PadFeedback
{
public:
    NxPadFeedback(NxPadSource& pads, HapticManager& haptics);

    void apply(const libchiaking::controllers::ControllerFeedback& feedback) override;

private:
    void applyTriggerEffects(const libchiaking::controllers::TriggerEffects& effects);
    void applyIntensity();

    NxPadSource& m_pads;
    HapticManager& m_haptics;
    std::uint8_t m_haptic_intensity = 1;
    std::uint8_t m_trigger_intensity = 1;
};

}
