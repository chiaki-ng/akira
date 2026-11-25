#pragma once

#include <settings/tomlmigrate.hpp>

namespace akira::settings::migrations {

inline void register_m003_drop_hardened_nat(tomlmigrate::Migrator& m) {
    m.step(3, "drop removed hardened_nat_traversal setting",
           [](toml::table& doc) {
        doc.erase("hardened_nat_traversal");
    });
}

}
