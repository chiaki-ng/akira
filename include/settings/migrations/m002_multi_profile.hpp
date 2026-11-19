#pragma once

#include <settings/tomlmigrate.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace chiaki_migrations {

inline void register_m002_multi_profile(tomlmigrate::Migrator& m) {
    m.step(2, "multi-profile: normalize flat account + name-keyed hosts into profiles/consoles/registrations",
           [](toml::table& doc) {
        if (doc.contains("profiles") || doc.contains("consoles") ||
            doc.contains("registrations"))
            return;

        auto get_str = [&](const toml::table& t, const char* key) -> std::string {
            auto v = t[key].value<std::string>();
            return v ? *v : std::string();
        };

        auto copy_as = [](const toml::table& src, toml::table& dst,
                          const char* srcKey, const char* dstKey) {
            auto it = src.find(srcKey);
            if (it == src.end()) return;
            it->second.visit([&](auto&& node) { dst.insert_or_assign(dstKey, node); });
        };

        auto is_known_table = [](std::string_view n) {
            return n == "rumble" || n == "picture_adjustments" || n == "button_mapping";
        };

        struct HostEntry { std::string name; toml::table tbl; };
        std::vector<HostEntry> hostEntries;
        for (auto&& [k, v] : doc) {
            if (!v.is_table()) continue;
            std::string name(k.str());
            if (is_known_table(name)) continue;
            const toml::table* t = v.as_table();
            if (!t->contains("host_type")) continue;
            hostEntries.push_back({name, *t});
        }

        bool anyRpKey = false;
        for (auto& h : hostEntries)
            if (h.tbl.contains("rp_key") && h.tbl.contains("rp_regist_key"))
                anyRpKey = true;

        const std::string gOnline  = get_str(doc, "psn_online_id");
        const std::string gAccount = get_str(doc, "psn_account_id");
        const bool haveGlobalAccount =
            !gOnline.empty() || !gAccount.empty() ||
            !get_str(doc, "psn_refresh_token").empty() ||
            !get_str(doc, "psn_access_token").empty() ||
            !get_str(doc, "global_duid").empty();

        toml::array profiles;
        std::vector<std::pair<std::string, int64_t>> keyToProfile;
        int64_t nextProfileId = 1;

        auto profile_key = [](const std::string& online, const std::string& account) {
            if (!account.empty()) return std::string("acct:") + account;
            if (!online.empty()) return std::string("online:") + online;
            return std::string();
        };

        auto mint_identity_profile = [&](const std::string& online,
                                         const std::string& account) -> int64_t {
            int64_t id = nextProfileId++;
            toml::table p;
            p.insert_or_assign("profile_id", id);
            if (!online.empty()) p.insert_or_assign("online_id", online);
            if (!account.empty()) p.insert_or_assign("account_id", account);
            profiles.push_back(std::move(p));
            std::string key = profile_key(online, account);
            if (!key.empty()) keyToProfile.emplace_back(key, id);
            return id;
        };

        int64_t defaultProfileId = 0;
        if (haveGlobalAccount || anyRpKey) {
            int64_t id = nextProfileId++;
            toml::table p;
            p.insert_or_assign("profile_id", id);
            copy_as(doc, p, "psn_online_id", "online_id");
            copy_as(doc, p, "psn_account_id", "account_id");
            copy_as(doc, p, "psn_refresh_token", "refresh_token");
            copy_as(doc, p, "psn_access_token", "access_token");
            copy_as(doc, p, "psn_token_expires_at", "token_expires_at");
            copy_as(doc, p, "psn_mobile_sso_refresh_token", "mobile_sso_refresh_token");
            copy_as(doc, p, "psn_mobile_sso_access_token", "mobile_sso_access_token");
            copy_as(doc, p, "psn_mobile_sso_expires_at", "mobile_sso_expires_at");
            copy_as(doc, p, "global_duid", "duid");
            profiles.push_back(std::move(p));
            defaultProfileId = id;
            std::string key = profile_key(gOnline, gAccount);
            if (!key.empty()) keyToProfile.emplace_back(key, id);
        }

        auto resolve_profile = [&](const std::string& online,
                                   const std::string& account) -> int64_t {
            std::string key = profile_key(online, account);
            if (key.empty())
                return defaultProfileId;
            for (auto& kv : keyToProfile)
                if (kv.first == key) return kv.second;
            return mint_identity_profile(online, account);
        };

        toml::array consoles;
        toml::array registrations;
        int64_t nextConsoleId = 1;

        for (auto& h : hostEntries) {
            int64_t consoleId = nextConsoleId++;

            toml::table c;
            c.insert_or_assign("console_id", consoleId);
            copy_as(h.tbl, c, "mac", "mac");
            c.insert_or_assign("nickname", h.name);
            copy_as(h.tbl, c, "host_addr", "host_addr");
            copy_as(h.tbl, c, "target", "target");
            copy_as(h.tbl, c, "host_type", "host_type");
            copy_as(h.tbl, c, "console_pin", "console_pin");
            copy_as(h.tbl, c, "haptic", "haptic");
            copy_as(h.tbl, c, "remote_duid", "remote_duid");
            consoles.push_back(std::move(c));

            if (!h.tbl.contains("rp_key") || !h.tbl.contains("rp_regist_key"))
                continue;

            int64_t profileId = resolve_profile(get_str(h.tbl, "psn_online_id"),
                                                get_str(h.tbl, "psn_account_id"));

            toml::table r;
            r.insert_or_assign("console_id", consoleId);
            r.insert_or_assign("profile_id", profileId);
            copy_as(h.tbl, r, "rp_key", "rp_key");
            copy_as(h.tbl, r, "rp_regist_key", "rp_regist_key");
            copy_as(h.tbl, r, "rp_key_type", "rp_key_type");
            copy_as(h.tbl, r, "server_mac", "server_mac");
            registrations.push_back(std::move(r));
        }

        for (const char* k : {"psn_online_id", "psn_account_id", "psn_refresh_token",
                              "psn_access_token", "psn_token_expires_at",
                              "psn_mobile_sso_refresh_token", "psn_mobile_sso_access_token",
                              "psn_mobile_sso_expires_at", "global_duid"})
            doc.erase(k);

        for (auto& h : hostEntries)
            doc.erase(h.name);

        if (!profiles.empty())
            doc.insert_or_assign("profiles", std::move(profiles));
        if (!consoles.empty())
            doc.insert_or_assign("consoles", std::move(consoles));
        if (!registrations.empty())
            doc.insert_or_assign("registrations", std::move(registrations));

        int64_t active = defaultProfileId;
        if (active == 0 && !keyToProfile.empty())
            active = keyToProfile.front().second;
        if (active != 0)
            doc.insert_or_assign("active_profile_id", active);
    });
}

}
