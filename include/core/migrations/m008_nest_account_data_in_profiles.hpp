#pragma once

#include <tomlmigrate/tomlmigrate.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <string>

#include <libchiaking/users_types.hpp>

#include "util/base64.hpp"

namespace chiaki_migrations {

inline void register_m008_nest_account_data_in_profiles(tomlmigrate::Migrator& m) {
    m.step(8, "move sign-ins and pairings under the profile that owns their account",
           [](toml::table& doc) {
        auto decimalAccount = [](const toml::table& profile) -> std::optional<std::string> {
            std::array<std::byte, libchiaking::psn_account_id_size> bytes{};
            if (!akira::base64::decodeExactly(profile["account_id"].value_or(std::string{}), bytes))
                return std::nullopt;
            const auto account = libchiaking::AccountId::from_bytes(bytes);
            if (account.value() == 0)
                return std::nullopt;
            return account.decimal();
        };

        auto ownedBy = [](const toml::array* rows, const std::string& account) {
            toml::array owned;
            if (!rows)
                return owned;
            for (const auto& node : *rows) {
                const auto* row = node.as_table();
                if (!row || (*row)["account"].value_or(std::string{}) != account)
                    continue;
                toml::table kept = *row;
                kept.erase("account");
                kept.erase("online_id");
                owned.push_back(std::move(kept));
            }
            return owned;
        };

        const auto* signIns = doc.get_as<toml::array>("sign_ins");
        const auto* pairings = doc.get_as<toml::array>("pairings");
        if (auto* profiles = doc.get_as<toml::array>("profiles")) {
            for (auto& node : *profiles) {
                auto* profile = node.as_table();
                if (!profile)
                    continue;
                const auto account = decimalAccount(*profile);
                if (!account)
                    continue;
                if (auto signIn = ownedBy(signIns, *account); !signIn.empty())
                    profile->insert_or_assign("sign_in", *signIn.front().as_table());
                if (auto owned = ownedBy(pairings, *account); !owned.empty())
                    profile->insert_or_assign("pairings", std::move(owned));
            }
        }
        doc.erase("sign_ins");
        doc.erase("pairings");
    });
}

}
