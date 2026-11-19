#ifndef AKIRA_SETTINGS_DESCRIPTIONS_HPP
#define AKIRA_SETTINGS_DESCRIPTIONS_HPP

#include <string>

namespace akira {

struct SettingDescription {
    std::string title;
    std::string body;
    std::string image;
};

bool lookupSettingDescription(const std::string& id, SettingDescription& out);

}

#endif // AKIRA_SETTINGS_DESCRIPTIONS_HPP
