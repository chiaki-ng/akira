#pragma once

#include <settings/tomlmigrate.hpp>

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace akira::settings::migrations {

inline bool ends_with(std::string_view s, std::string_view suf) {
    return s.size() >= suf.size() && s.substr(s.size() - suf.size()) == suf;
}

inline bool starts_with(std::string_view s, std::string_view pre) {
    return s.size() >= pre.size() && s.substr(0, pre.size()) == pre;
}

inline void copy_if_absent(toml::table& doc, const char* from, const char* to) {
    auto src = doc.find(from);
    if (src == doc.end() || doc.contains(to))
        return;
    src->second.visit([&](auto&& node) { doc.insert_or_assign(to, node); });
}

inline bool truthy(const toml::table& doc, const char* key) {
    return doc[key].is_boolean() && doc[key].value<bool>().value_or(false);
}

inline void register_m000_baseline(tomlmigrate::Migrator& m) {
    m.step(0, "baseline: converge all pre-versioning config shapes",
           [](toml::table& doc) {
        copy_if_absent(doc, "video_resolution", "local_video_resolution");
        copy_if_absent(doc, "video_resolution", "remote_video_resolution");
        copy_if_absent(doc, "video_fps", "local_video_fps");
        copy_if_absent(doc, "video_fps", "remote_video_fps");
        copy_if_absent(doc, "video_bitrate", "local_video_bitrate");
        copy_if_absent(doc, "video_bitrate", "remote_video_bitrate");
        copy_if_absent(doc, "power_user_mode", "power_user_menu_unlocked");
        copy_if_absent(doc, "fsr_sharpness", "rcas_sharpness");

        const bool any_modern_fsr = doc.contains("local_fsr_enabled") ||
                                    doc.contains("remote_fsr_enabled") ||
                                    doc.contains("vpn_fsr_enabled");
        if (!any_modern_fsr &&
            (truthy(doc, "easu_enabled") || truthy(doc, "fsr_enabled"))) {
            doc.insert_or_assign("local_fsr_enabled", true);
            doc.insert_or_assign("remote_fsr_enabled", true);
            doc.insert_or_assign("vpn_fsr_enabled", true);
        }
        if (!doc.contains("rcas_enabled") && truthy(doc, "fsr_enabled"))
            doc.insert_or_assign("rcas_enabled", true);

        for (const char* k : {"invert_ab", "enable_experimental_crypto",
                              "video_resolution", "video_fps", "video_bitrate",
                              "power_user_mode", "low_latency_mode",
                              "easu_enabled", "fsr_enabled", "fsr_sharpness"})
            doc.erase(k);

        static const std::array<std::string_view, 3> known_tables = {
            "rumble", "picture_adjustments", "button_mapping"};
        auto is_known = [](std::string_view n) {
            for (auto k : known_tables)
                if (k == n) return true;
            return false;
        };

        struct Fix {
            std::string old_name;
            std::string clean_name;
            std::int64_t type;
        };
        std::vector<Fix> fixes;
        for (auto&& [k, v] : doc) {
            if (!v.is_table()) continue;
            std::string name(k.str());
            if (is_known(name)) continue;
            if (v.as_table()->contains("host_type")) continue;

            std::string clean = name;
            std::int64_t type = 0;
            if (ends_with(name, " (Remote)")) {
                type = 3;
            } else if (starts_with(name, "[Manual] ")) {
                clean = name.substr(9);
                type = 2;
            } else if (starts_with(name, "[Auto] ")) {
                clean = name.substr(7);
                type = 1;
            }
            fixes.push_back({name, clean, type});
        }

        for (auto& f : fixes) {
            auto* t = doc.get_as<toml::table>(f.old_name);
            if (!t) continue;
            t->insert_or_assign("host_type", f.type);
            if (f.clean_name == f.old_name) continue;

            if (auto* existing = doc.get_as<toml::table>(f.clean_name)) {
                std::int64_t ex_type =
                    (*existing)["host_type"].value<std::int64_t>().value_or(0);
                if (ex_type == 2 && f.type != 2) {
                    doc.erase(f.old_name);
                    continue;
                }
            }
            toml::table copy = *t;
            doc.erase(f.old_name);
            doc.insert_or_assign(f.clean_name, std::move(copy));
        }
    });
}

}
