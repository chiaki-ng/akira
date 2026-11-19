#pragma once

#include <settings/tomlmigrate.hpp>

#include <cstdint>
#include <string>

namespace chiaki_migrations {

inline void register_m010_record_fsr_targets(tomlmigrate::Migrator& m) {
    m.step(10, "record the fsr target height of each video profile",
           [](toml::table& doc) {
        auto* video = doc.get_as<toml::table>("video");
        if (!video)
            return;
        for (const char* name : {"local", "remote", "vpn"}) {
            auto* profile = video->get_as<toml::table>(name);
            if (!profile || profile->contains("fsr_target") || !(*profile)["fsr_enabled"].value_or(false))
                continue;
            const std::string resolution = (*profile)["resolution"].value_or(std::string());
            const std::int64_t target = resolution == "540p" ? 720 : resolution == "720p" ? 1080 : 0;
            if (target > 0)
                profile->insert("fsr_target", target);
        }
    });
}

}
