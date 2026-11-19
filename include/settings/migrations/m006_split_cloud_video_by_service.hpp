#pragma once

#include <settings/tomlmigrate.hpp>

namespace chiaki_migrations {

inline void register_m006_split_cloud_video_by_service(tomlmigrate::Migrator& m) {
    m.step(6, "give pscloud and psnow their own cloud video settings",
           [](toml::table& doc) {
        auto* video = doc.get_as<toml::table>("video");
        if (!video)
            return;

        auto* cloud = video->get_as<toml::table>("cloud");
        if (!cloud || cloud->contains("pscloud") || cloud->contains("psnow"))
            return;

        toml::table shared;
        for (const char* key : {"resolution", "bitrate", "fsr_enabled"}) {
            auto it = cloud->find(key);
            if (it == cloud->end())
                continue;
            it->second.visit([&](auto&& node) { shared.insert_or_assign(key, node); });
            cloud->erase(key);
        }

        if (shared.empty())
            return;

        cloud->insert_or_assign("pscloud", shared);
        cloud->insert_or_assign("psnow", shared);
    });
}

}
