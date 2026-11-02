#include "core/profile_tables.hpp"

#include <string_view>

namespace akira {

auto ProfileTables::accountOf(const toml::table& profile) -> std::optional<Account>
{
    return Account::from_base64(profile["account_id"].value_or(std::string_view{}));
}

void ProfileTables::each(const toml::table& config, const std::function<void(Account, const toml::table&)>& visit)
{
    const auto* profiles = config["profiles"].as_array();
    if (!profiles)
        return;
    for (const auto& node : *profiles) {
        const auto* profile = node.as_table();
        if (!profile)
            continue;
        if (const auto account = accountOf(*profile))
            visit(*account, *profile);
    }
}

void ProfileTables::each(toml::table& config, const std::function<void(Account, toml::table&)>& visit)
{
    auto* profiles = config["profiles"].as_array();
    if (!profiles)
        return;
    for (auto& node : *profiles) {
        auto* profile = node.as_table();
        if (!profile)
            continue;
        if (const auto account = accountOf(*profile))
            visit(*account, *profile);
    }
}

}
