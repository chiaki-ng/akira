#ifndef AKIRA_SETTINGS_DESCRIPTIONS_HPP
#define AKIRA_SETTINGS_DESCRIPTIONS_HPP

#include <string>

namespace akira::platform {
class PlatformFactory;
}

namespace akira::settings {

struct SettingDescription {
    std::string title;
    std::string body;
    std::string image;
};

bool lookupSettingDescription(platform::PlatformFactory& platform, const std::string& id, SettingDescription& out);

}

#endif // AKIRA_SETTINGS_DESCRIPTIONS_HPP
