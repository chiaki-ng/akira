#pragma once

#include <tomlmigrate/tomlmigrate.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "util/base64.hpp"

namespace chiaki_migrations {

inline void register_m007_split_psn_sign_ins(tomlmigrate::Migrator& m) {
    m.step(7, "move PSN tokens out of profiles into sign-ins keyed by account",
           [](toml::table& doc) {
        auto* profiles = doc.get_as<toml::array>("profiles");
        if (!profiles)
            return;

        auto decimalAccount = [](const std::string& base64) -> std::optional<std::string> {
            std::array<std::byte, 8> bytes{};
            if (!akira::base64::decodeExactly(base64, bytes))
                return std::nullopt;
            std::uint64_t value = 0;
            for (std::size_t i = 0; i < bytes.size(); ++i)
                value |= std::to_integer<std::uint64_t>(bytes[i]) << (8 * i);
            if (value == 0)
                return std::nullopt;
            return std::to_string(value);
        };

        auto copyText = [](const toml::table& from, const char* key, toml::table& to, const char* as) {
            auto text = from[key].value<std::string>();
            if (text && !text->empty())
                to.insert_or_assign(as, *text);
        };

        auto copySeconds = [](const toml::table& from, const char* key, toml::table& to, const char* as) {
            auto seconds = from[key].value<std::int64_t>();
            if (seconds && *seconds > 0)
                to.insert_or_assign(as, *seconds * 1000);
        };

        toml::array signIns;
        if (const auto* existing = doc.get_as<toml::array>("sign_ins"))
            signIns = *existing;

        for (auto& node : *profiles) {
            auto* profile = node.as_table();
            if (!profile)
                continue;

            const auto npsso = (*profile)["npsso"].value<std::string>();
            const auto account = decimalAccount((*profile)["account_id"].value_or(std::string{}));
            const bool legacy = (*profile)["legacy"].value_or(false);
            if (npsso && !npsso->empty() && account && !legacy) {
                toml::table signIn;
                signIn.insert_or_assign("account", *account);
                copyText(*profile, "online_id", signIn, "online_id");
                signIn.insert_or_assign("npsso", *npsso);
                copyText(*profile, "access_token", signIn, "remote_play_token");
                copySeconds(*profile, "token_expires_at", signIn, "remote_play_token_expires_at");
                copyText(*profile, "refresh_token", signIn, "remote_play_refresh_token");
                copyText(*profile, "mobile_sso_access_token", signIn, "app_token");
                copySeconds(*profile, "mobile_sso_expires_at", signIn, "app_token_expires_at");
                copyText(*profile, "mobile_sso_refresh_token", signIn, "app_refresh_token");
                copyText(*profile, "duid", signIn, "device_id");
                signIns.push_back(std::move(signIn));
            }

            for (const char* key : {"refresh_token", "access_token", "token_expires_at",
                                    "mobile_sso_refresh_token", "mobile_sso_access_token",
                                    "mobile_sso_expires_at", "npsso", "npsso_last_checked_at",
                                    "npsso_valid", "duid"})
                profile->erase(key);
        }

        if (!signIns.empty())
            doc.insert_or_assign("sign_ins", std::move(signIns));
    });
}

}
