#include "platform/feedback/extended/provider.hpp"

#include <cstring>

#include "platform/pads/extended/ps_output.hpp"
#include "play/haptic_manager.hpp"
#include "platform/pads/extended/provider.hpp"

namespace akira::platform::extended
{

ExtendedFeedbackProvider::ExtendedFeedbackProvider(ExtendedPadProvider& pads, play::HapticManager& haptics)
    : m_pads(pads)
    , m_haptics(haptics)
{
}

void ExtendedFeedbackProvider::apply(const libchiaking::controllers::ControllerFeedback& feedback)
{
    if (const auto* effects = feedback.get<libchiaking::controllers::TriggerEffects>())
    {
        applyTriggerEffects(*effects);
    }
    else if (const auto* haptic = feedback.get<libchiaking::controllers::HapticIntensity>())
    {
        m_haptic_intensity = static_cast<std::uint8_t>(haptic->level);
        applyIntensity();
    }
    else if (const auto* trigger = feedback.get<libchiaking::controllers::TriggerIntensity>())
    {
        m_trigger_intensity = static_cast<std::uint8_t>(trigger->level);
        applyIntensity();
    }
    else if (const auto* light = feedback.get<libchiaking::controllers::Lightbar>())
    {
        m_pads.sendLightbar(light->colour.red, light->colour.green, light->colour.blue);
    }
}

void ExtendedFeedbackProvider::applyTriggerEffects(const libchiaking::controllers::TriggerEffects& effects)
{
    if (play::Ds5IntensityFromWire(m_trigger_intensity) == play::Ds5EffectIntensity::Off)
        return;

    PadPath::TriggerEffect left;
    PadPath::TriggerEffect right;

    left.type = effects.left.mode;
    std::memcpy(left.params, effects.left.parameters.data(), sizeof(left.params));
    right.type = effects.right.mode;
    std::memcpy(right.params, effects.right.parameters.data(), sizeof(right.params));

    m_pads.sendTriggerEffects(left, right);
}

void ExtendedFeedbackProvider::applyIntensity()
{
    m_pads.sendEffectIntensity(m_haptic_intensity, m_trigger_intensity);
    m_haptics.setConsoleVibration(m_haptic_intensity);
}

}
