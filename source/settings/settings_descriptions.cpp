#include "settings/settings_descriptions.hpp"

#include <borealis/core/application.hpp>
#include <borealis/extern/nlohmann/json.hpp>

#include "platform/factory.hpp"

#include <array>
#include <fstream>
#include <span>
#include <string>
#include <unordered_map>

namespace akira::settings {

static const auto kHelpFiles = std::to_array<const char*>({
    "general", "quality", "controls", "debug", "account", "poweruser", "developer",
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

static const std::unordered_map<std::string, SettingDescription>& table(akira::platform::PlatformFactory& platform)
{
    static const std::unordered_map<std::string, SettingDescription> t = [&platform]() {
        std::unordered_map<std::string, SettingDescription> m;

        std::string locale = brls::Application::getLocale();

        for (const auto& names : {std::span<const char* const>(kHelpFiles),
                                  platform.helpFiles(), platform.updates().helpFiles()}) {
            for (const char* name : names) {
                mergeHelpFile(std::string("romfs:/help/") + name + ".json", m);
                if (!locale.empty() && locale != "en-US")
                    mergeHelpFile(std::string("romfs:/help/") + locale + "/" + name + ".json", m);
            }
        }
        return m;
    }();
    return t;
}

bool lookupSettingDescription(platform::PlatformFactory& platform, const std::string& id, SettingDescription& out)
{
    const auto& t = table(platform);
    auto it = t.find(id);
    if (it == t.end())
        return false;
    out = it->second;
    return true;
}

}
