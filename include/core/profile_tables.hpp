#pragma once

#include <functional>
#include <optional>

#include <libchiaking/users_types.hpp>
#include <toml++/toml.hpp>

namespace akira {

class ProfileTables {
public:
    using Account = libchiaking::AccountId;

    static auto accountOf(const toml::table& profile) -> std::optional<Account>;
    static void each(const toml::table& config, const std::function<void(Account, const toml::table&)>& visit);
    static void each(toml::table& config, const std::function<void(Account, toml::table&)>& visit);
};

}
