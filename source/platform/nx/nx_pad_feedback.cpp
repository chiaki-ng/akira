#include "platform/nx/nx_pad_feedback.hpp"

#include <cstring>

#include "input/ps_output.hpp"
#include "stream/haptic_manager.hpp"
#include "stream/input_manager.hpp"

namespace akira::platform
{

NxPadFeedback::NxPadFeedback(InputManager& input, HapticManager& haptics)
    : m_input(input)
    , m_haptics(haptics)
{
}

void NxPadFeedback::apply(const libchiaking::controllers::ControllerFeedback& feedback)
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
        m_input.sendLightbar(light->colour.red, light->colour.green, light->colour.blue);
    }
}

void NxPadFeedback::applyTriggerEffects(const libchiaking::controllers::TriggerEffects& effects)
{
    if (input::Ds5IntensityFromWire(m_trigger_intensity) == input::Ds5EffectIntensity::Off)
        return;

    input::PadPath::TriggerEffect left;
    input::PadPath::TriggerEffect right;

    left.type = effects.left.mode;
    std::memcpy(left.params, effects.left.parameters.data(), sizeof(left.params));
    right.type = effects.right.mode;
    std::memcpy(right.params, effects.right.parameters.data(), sizeof(right.params));

    m_input.sendTriggerEffects(left, right);
}

void NxPadFeedback::applyIntensity()
{
    m_input.sendEffectIntensity(m_haptic_intensity, m_trigger_intensity);
    m_haptics.setConsoleVibration(m_haptic_intensity);
}

}
