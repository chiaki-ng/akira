#include "platform/pad_settings/hid/provider.hpp"

#include "settings/settings_manager.hpp"

namespace akira::platform::hid
{

HidPadSettingsProvider::HidPadSettingsProvider(const input::PadDescription& pad)
    : m_pad(pad)
{
}

std::string HidPadSettingsProvider::resolveKey(const SettingsManager& settings, bool joycon)
{
    const std::string category = joycon ? play::kRumbleKeyJoyCon : play::kRumbleKeySwitchPro;
    if (settings.hasRumbleProfile(category))
        return category;

    return play::kRumbleKeyDefault;
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
