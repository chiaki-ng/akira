#pragma once

#include <tomlmigrate/tomlmigrate.hpp>

#include <cstdint>

namespace chiaki_migrations {

inline void register_m009_split_takion_version_by_target(tomlmigrate::Migrator& m) {
    m.step(9, "split the Takion version override into one per stream target",
           [](toml::table& doc) {
        auto* stream = doc.get_as<toml::table>("stream");
        if (!stream)
            return;
        const auto version = (*stream)["takion_version"].value<std::int64_t>();
        stream->erase("takion_version");
        if (!version || *version <= 0)
            return;
        toml::table versions;
        for (const char* target : {"ps4", "ps5", "ps5_cloud", "ps_now"})
            versions.insert(target, *version);
        stream->insert_or_assign("takion_versions", std::move(versions));
    });
}

}
