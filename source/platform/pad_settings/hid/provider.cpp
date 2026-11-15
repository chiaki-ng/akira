#include "platform/pad_settings/hid/provider.hpp"

#include "core/settings_manager.hpp"

namespace akira::platform::hid
{

HidPadSettingsProvider::HidPadSettingsProvider(const input::PadDescription& pad)
    : m_pad(pad)
{
}

std::string HidPadSettingsProvider::resolveKey(const SettingsManager& settings, bool joycon)
{
    const std::string category = joycon ? input::kRumbleKeyJoyCon : input::kRumbleKeySwitchPro;
    if (settings.hasRumbleProfile(category))
        return category;

    return input::kRumbleKeyDefault;
}

std::string HidPadSettingsProvider::rumbleKey(SettingsManager& settings)
{
    return currentKey(settings);
}

std::string HidPadSettingsProvider::currentKey(const SettingsManager& settings) const
{
    return resolveKey(settings, m_pad.kind == input::PadPathKind::JoyCon);
}

}
