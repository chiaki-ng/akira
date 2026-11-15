#include "platform/ounce/ounce_pad_feedback.hpp"

#include "stream/haptic_manager.hpp"

namespace akira::platform
{

OuncePadFeedback::OuncePadFeedback(HapticManager& haptics)
    : m_haptics(haptics)
{
}

void OuncePadFeedback::apply(const libchiaking::controllers::ControllerFeedback& feedback)
{
    if (const auto* haptic = feedback.get<libchiaking::controllers::HapticIntensity>())
        m_haptics.setConsoleVibration(static_cast<uint8_t>(haptic->level));
}

}
