#include "core/settings_descriptions.hpp"

#include <borealis/core/application.hpp>
#include <borealis/extern/nlohmann/json.hpp>

#include <array>
#include <fstream>
#include <string>
#include <unordered_map>

namespace akira {

static const auto kHelpFiles = std::to_array<const char*>({
    "general", "quality", "controls", "debug", "account", "poweruser", "developer",
#if AKIRA_PLATFORM_NX
    "updates",
#endif
#if AKIRA_PLATFORM_OUNCE
    "ounce",
#endif
});

static void mergeHelpFile(const std::string& path, std::unordered_map<std::string, SettingDescription>& m)
{
    std::ifstream file(path);
    if (!file.is_open())
        return;

    nlohmann::json j;
    try {
        file >> j;
    } catch (...) {
        return;
    }

    for (auto it = j.begin(); it != j.end(); ++it) {
        const auto& v = it.value();
        SettingDescription d;
        d.title = v.value("title", std::string());
        d.body  = v.value("body", std::string());
        d.image = v.value("image", std::string());
        m[it.key()] = d;
    }
}

static const std::unordered_map<std::string, SettingDescription>& table()
{
    static const std::unordered_map<std::string, SettingDescription> t = []() {
        std::unordered_map<std::string, SettingDescription> m;

        std::string locale = brls::Application::getLocale();

        for (const char* name : kHelpFiles) {
            mergeHelpFile(std::string("romfs:/help/") + name + ".json", m);
            if (!locale.empty() && locale != "en-US")
                mergeHelpFile(std::string("romfs:/help/") + locale + "/" + name + ".json", m);
        }
        return m;
    }();
    return t;
}

bool lookupSettingDescription(const std::string& id, SettingDescription& out)
{
    const auto& t = table();
    auto it = t.find(id);
    if (it == t.end())
        return false;
    out = it->second;
    return true;
}

}
