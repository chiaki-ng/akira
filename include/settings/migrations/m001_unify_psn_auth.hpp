#pragma once

#include <settings/tomlmigrate.hpp>

#include <string>

namespace chiaki_migrations {

inline void register_m001_unify_psn_auth(tomlmigrate::Migrator& m) {
    m.step(1, "unify psn auth: promote per-host creds to global, backfill per-host",
           [](toml::table& doc) {
        auto is_host = [](const toml::node& v) {
            const toml::table* t = v.as_table();
            return t && t->contains("host_type");
        };

        auto global_empty = [&](const char* key) {
            auto v = doc[key].value<std::string>();
            return !v || v->empty();
        };

        auto promote = [&](const char* key) {
            if (!global_empty(key)) return;
            for ([[maybe_unused]] auto&& [k, v] : doc) {
                if (!is_host(v)) continue;
                auto s = (*v.as_table())[key].value<std::string>();
                if (s && !s->empty()) {
                    doc.insert_or_assign(key, *s);
                    return;
                }
            }
        };

        auto backfill = [&](const char* key) {
            auto g = doc[key].value<std::string>();
            if (!g || g->empty()) return;
            for ([[maybe_unused]] auto&& [k, v] : doc) {
                if (!is_host(v)) continue;
                toml::table* t = v.as_table();
                auto cur = (*t)[key].value<std::string>();
                if (!cur || cur->empty())
                    t->insert_or_assign(key, *g);
            }
        };

        promote("psn_account_id");
        promote("psn_online_id");
        backfill("psn_account_id");
        backfill("psn_online_id");
    });
}

}
