#pragma once

#include <settings/tomlmigrate.hpp>

namespace akira::settings::migrations {

inline void register_m011_default_drako3d_renderer(tomlmigrate::Migrator& m) {
    m.step(11, "make drako3d the renderer",
           [](toml::table& doc) {
        auto* video = doc.get_as<toml::table>("video");
        if (!video)
            return;
        video->insert_or_assign("renderer", "drako3d");
    });
}

}
