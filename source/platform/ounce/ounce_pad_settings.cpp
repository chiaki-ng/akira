#include "platform/ounce/ounce_pad_settings.hpp"

#include "core/settings_manager.hpp"

namespace akira::platform
{

OuncePadSettings::OuncePadSettings(const input::PadDescription& pad)
    : m_pad(pad)
{
}

std::string OuncePadSettings::resolveKey(const SettingsManager& settings, bool joycon)
{
    const std::string category = joycon ? input::kRumbleKeyJoyCon : input::kRumbleKeySwitchPro;
    if (settings.hasRumbleProfile(category))
        return category;

    return input::kRumbleKeyDefault;
}

std::string OuncePadSettings::rumbleKey(SettingsManager& settings)
{
    return currentKey(settings);
}

std::string OuncePadSettings::currentKey(const SettingsManager& settings) const
{
    return resolveKey(settings, m_pad.kind == input::PadPathKind::JoyCon);
}

}
