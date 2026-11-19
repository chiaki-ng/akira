#pragma once

#include <settings/tomlmigrate.hpp>

#include <json-c/json.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace chiaki_migrations {

inline void move_key(toml::table& doc, const char* from, toml::table& dest, const char* to) {
    auto it = doc.find(from);
    if (it == doc.end())
        return;
    it->second.visit([&](auto&& node) { dest.insert_or_assign(to, node); });
    doc.erase(from);
}

inline void move_table(toml::table& doc, const char* from, toml::table& dest, const char* to) {
    auto* src = doc.get_as<toml::table>(from);
    if (!src)
        return;
    dest.insert_or_assign(to, *src);
    doc.erase(from);
}

inline void adopt(toml::table& doc, const char* name, toml::table t) {
    if (!t.empty())
        doc.insert_or_assign(name, std::move(t));
}

inline toml::array shortcut_rows_from_json(const std::string& json) {
    toml::array rows;
    json_object* root = json_tokener_parse(json.c_str());
    if (!root)
        return rows;

    if (json_object_is_type(root, json_type_array)) {
        auto str_field = [](json_object* obj, const char* key) -> std::string {
            json_object* v = nullptr;
            if (!json_object_object_get_ex(obj, key, &v) || !v)
                return "";
            const char* s = json_object_get_string(v);
            return s ? s : "";
        };
        auto bool_field = [](json_object* obj, const char* key) -> bool {
            json_object* v = nullptr;
            if (!json_object_object_get_ex(obj, key, &v) || !v)
                return false;
            return json_object_get_boolean(v);
        };

        static const std::pair<const char*, const char*> stringFields[] = {
            {"imageUrl", "image_url"},
            {"landscapeImageUrl", "landscape_image_url"},
            {"conceptId", "concept_id"},
            {"category", "category"},
            {"serviceType", "service_type"},
            {"platform", "platform"},
            {"streamServiceType", "stream_service_type"},
            {"streamIdentifier", "stream_identifier"},
            {"entitlementId", "entitlement_id"},
            {"storeProductId", "store_product_id"},
            {"conceptUrl", "concept_url"},
        };

        size_t count = json_object_array_length(root);
        for (size_t i = 0; i < count; i++) {
            json_object* item = json_object_array_get_idx(root, i);
            if (!item || !json_object_is_type(item, json_type_object))
                continue;

            std::string productId = str_field(item, "productId");
            std::string name = str_field(item, "name");
            if (productId.empty() || name.empty())
                continue;

            toml::table t;
            t.insert("product_id", productId);
            t.insert("name", name);
            for (const auto& [jsonKey, tomlKey] : stringFields) {
                std::string v = str_field(item, jsonKey);
                if (!v.empty())
                    t.insert(tomlKey, v);
            }
            if (bool_field(item, "isOwned"))
                t.insert("is_owned", true);
            if (bool_field(item, "plusCatalog"))
                t.insert("plus_catalog", true);
            rows.push_back(std::move(t));
        }
    }

    json_object_put(root);
    return rows;
}

inline void register_m005_group_settings_tables(tomlmigrate::Migrator& m) {
    m.step(5, "group the flat settings keys into domain tables",
           [](toml::table& doc) {
        auto video_profile = [&](const char* resolution, const char* fps,
                                 const char* bitrate, const char* fsr) {
            toml::table t;
            if (resolution) move_key(doc, resolution, t, "resolution");
            if (fps) move_key(doc, fps, t, "fps");
            if (bitrate) move_key(doc, bitrate, t, "bitrate");
            if (fsr) move_key(doc, fsr, t, "fsr_enabled");
            return t;
        };

        toml::table video;
        adopt(video, "local", video_profile("local_video_resolution", "local_video_fps",
                                            "local_video_bitrate", "local_fsr_enabled"));
        adopt(video, "remote", video_profile("remote_video_resolution", "remote_video_fps",
                                             "remote_video_bitrate", "remote_fsr_enabled"));
        adopt(video, "vpn", video_profile("vpn_video_resolution", "vpn_video_fps",
                                          "vpn_video_bitrate", "vpn_fsr_enabled"));
        adopt(video, "cloud", video_profile("cloud_video_resolution", nullptr,
                                            "cloud_video_bitrate", "cloud_fsr_enabled"));
        adopt(doc, "video", std::move(video));

        toml::table picture;
        if (auto* adjustments = doc.get_as<toml::table>("picture_adjustments")) {
            move_key(*adjustments, "enable_dithering", picture, "dithering_enabled");
            move_key(*adjustments, "dithering_strength", picture, "dithering_strength");
            doc.erase("picture_adjustments");
        }
        move_key(doc, "rcas_enabled", picture, "rcas_enabled");
        move_key(doc, "rcas_sharpness", picture, "rcas_sharpness");
        adopt(doc, "picture", std::move(picture));

        toml::table input;
        move_key(doc, "haptic", input, "haptic");
        move_key(doc, "gyro_source", input, "gyro_source");
        move_table(doc, "rumble", input, "rumble");
        move_table(doc, "button_mapping", input, "button_mapping");
        adopt(doc, "input", std::move(input));

        toml::table cloud;
        move_key(doc, "cloud_datacenter_pscloud", cloud, "datacenter_pscloud");
        move_key(doc, "cloud_datacenter_psnow", cloud, "datacenter_psnow");
        move_key(doc, "cloud_sort_state", cloud, "sort_state");
        move_key(doc, "cloud_attr_passed", cloud, "attr_passed");

        if (auto favorites = doc["cloud_favorites"].value<std::string>()) {
            toml::array ids;
            size_t start = 0;
            while (start <= favorites->size()) {
                size_t nl = favorites->find('\n', start);
                std::string id = favorites->substr(
                    start, nl == std::string::npos ? std::string::npos : nl - start);
                if (!id.empty())
                    ids.push_back(id);
                if (nl == std::string::npos)
                    break;
                start = nl + 1;
            }
            if (!ids.empty())
                cloud.insert("favorites", std::move(ids));
        }
        doc.erase("cloud_favorites");

        toml::table datacenters;
        move_key(doc, "cloud_datacenters_pscloud", datacenters, "pscloud");
        move_key(doc, "cloud_datacenters_psnow", datacenters, "psnow");
        adopt(cloud, "datacenters", std::move(datacenters));
        adopt(doc, "cloud", std::move(cloud));

        toml::table network;
        move_key(doc, "holepunch_retry", network, "holepunch_retry");
        move_key(doc, "port_guessing", network, "port_guessing");
        move_key(doc, "port_guessing_count", network, "port_guessing_count");
        move_key(doc, "port_guessing_socks", network, "port_guessing_socks");
        move_key(doc, "discovery_subnets", network, "discovery_subnets");
        move_key(doc, "companion_port", network, "companion_port");
        adopt(doc, "network", std::move(network));

        toml::table stream;
        move_key(doc, "auto_reconnect", stream, "auto_reconnect");
        move_key(doc, "sleep_on_exit", stream, "sleep_on_exit");
        move_key(doc, "request_idr_on_fec_failure", stream, "request_idr_on_fec_failure");
        move_key(doc, "packet_loss_max", stream, "packet_loss_max");
        adopt(doc, "stream", std::move(stream));

        toml::table ui;
        move_key(doc, "ui_theme", ui, "theme");
        move_key(doc, "hide_account_name", ui, "hide_account_name");
        move_key(doc, "connection_show_stages", ui, "connection_show_stages");
        adopt(doc, "ui", std::move(ui));

        toml::table psn;
        move_key(doc, "psn_request_budget", psn, "request_budget");
        move_key(doc, "psn_request_window_seconds", psn, "request_window_seconds");
        adopt(doc, "psn", std::move(psn));

        toml::table updates;
        move_key(doc, "update_channel", updates, "channel");
        move_key(doc, "auto_check_updates", updates, "auto_check");
        move_key(doc, "last_update_check", updates, "last_check");
        move_key(doc, "update_install_path", updates, "install_path");
        adopt(doc, "updates", std::move(updates));

        toml::table debug;
        move_key(doc, "debug_locale", debug, "locale");
        move_key(doc, "enable_file_logging", debug, "file_logging");
        move_key(doc, "enable_thread_affinity", debug, "thread_affinity");
        move_key(doc, "debug_lwip_log", debug, "lwip_log");
        move_key(doc, "debug_wireguard_log", debug, "wireguard_log");
        move_key(doc, "debug_render_log", debug, "render_log");
        move_key(doc, "debug_chiaki_log", debug, "chiaki_log");
        move_key(doc, "debug_discovery_log", debug, "discovery_log");
        move_key(doc, "debug_ffmpeg_log", debug, "ffmpeg_log");
        move_key(doc, "ipc_stats_enabled", debug, "ipc_stats");
        move_key(doc, "dev_fake_hosts", debug, "fake_hosts");
        move_key(doc, "power_user_menu_unlocked", debug, "power_user_menu_unlocked");
        move_key(doc, "unlock_bitrate_max", debug, "unlock_bitrate_max");
        move_key(doc, "dev_update_server", debug, "update_server");
        move_key(doc, "dev_force_ws_fqdn", debug, "force_ws_fqdn");
        adopt(doc, "debug", std::move(debug));

        // Shortcuts: per-profile JSON blob, plus the pre-multi-profile global one that
        // parseTomlFile used to hand to the active profile at load time.
        std::string globalShortcuts = doc["cloud_shortcuts"].value<std::string>().value_or("");
        doc.erase("cloud_shortcuts");
        const int64_t activeProfile = doc["active_profile_id"].value<int64_t>().value_or(0);

        if (auto* profiles = doc.get_as<toml::array>("profiles")) {
            for (auto& elem : *profiles) {
                auto* pt = elem.as_table();
                if (!pt)
                    continue;

                std::string blob = (*pt)["cloud_shortcuts"].value<std::string>().value_or("");
                if (blob.empty() && !globalShortcuts.empty() &&
                    (*pt)["profile_id"].value<int64_t>().value_or(0) == activeProfile)
                    blob = globalShortcuts;

                pt->erase("cloud_shortcuts");
                toml::array rows = shortcut_rows_from_json(blob);
                if (!rows.empty())
                    pt->insert("cloud_shortcuts", std::move(rows));
            }
        }
    });
}

}
