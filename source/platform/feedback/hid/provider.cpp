#include "platform/feedback/hid/provider.hpp"

#include "stream/haptic_manager.hpp"

namespace akira::platform::hid
{

HidFeedbackProvider::HidFeedbackProvider(HapticManager& haptics)
    : m_haptics(haptics)
{
}

void HidFeedbackProvider::apply(const libchiaking::controllers::ControllerFeedback& feedback)
{
    if (const auto* haptic = feedback.get<libchiaking::controllers::HapticIntensity>())
        m_haptics.setConsoleVibration(static_cast<uint8_t>(haptic->level));
}

}
