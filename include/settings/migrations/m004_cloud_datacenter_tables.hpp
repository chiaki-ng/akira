#pragma once

#include <settings/tomlmigrate.hpp>

#include <json-c/json.h>

#include <string>

namespace chiaki_migrations {

inline toml::array datacenter_rows_from_json(const std::string& json) {
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
        auto int_field = [](json_object* obj, const char* key) -> int64_t {
            json_object* v = nullptr;
            if (!json_object_object_get_ex(obj, key, &v) || !v)
                return 0;
            return json_object_get_int64(v);
        };
        auto bool_field = [](json_object* obj, const char* key) -> bool {
            json_object* v = nullptr;
            if (!json_object_object_get_ex(obj, key, &v) || !v)
                return false;
            return json_object_get_boolean(v);
        };

        size_t count = json_object_array_length(root);
        for (size_t i = 0; i < count; i++) {
            json_object* item = json_object_array_get_idx(root, i);
            if (!item || !json_object_is_type(item, json_type_object))
                continue;

            std::string name = str_field(item, "dataCenter");
            if (name.empty())
                continue;

            toml::table t;
            t.insert("name", name);
            t.insert("rtt", int_field(item, "rtt"));

            toml::array rtts;
            json_object* samples = nullptr;
            if (json_object_object_get_ex(item, "rtts", &samples) && samples &&
                json_object_is_type(samples, json_type_array)) {
                size_t sampleCount = json_object_array_length(samples);
                for (size_t j = 0; j < sampleCount; j++) {
                    json_object* sample = json_object_array_get_idx(samples, j);
                    if (sample)
                        rtts.push_back(json_object_get_int64(sample));
                }
            }
            t.insert("rtts", rtts);

            t.insert("mtu_in", int_field(item, "mtu_in"));
            t.insert("mtu_out", int_field(item, "mtu_out"));
            t.insert("port", int_field(item, "port"));
            t.insert("public_ip", str_field(item, "publicIp"));
            t.insert("max_bandwidth", int_field(item, "maxBandwidth"));
            t.insert("measured", bool_field(item, "measured"));
            rows.push_back(std::move(t));
        }
    }

    json_object_put(root);
    return rows;
}

inline void register_m004_cloud_datacenter_tables(tomlmigrate::Migrator& m) {
    m.step(4, "explode the cloud datacenter JSON blobs into toml tables",
           [](toml::table& doc) {
        for (const char* key : {"cloud_datacenters_pscloud", "cloud_datacenters_psnow"}) {
            auto blob = doc[key].value<std::string>();
            if (!blob)
                continue;

            toml::array rows = datacenter_rows_from_json(*blob);
            if (rows.empty())
                doc.erase(key);
            else
                doc.insert_or_assign(key, std::move(rows));
        }
    });
}

}
