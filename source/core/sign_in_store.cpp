#include "core/sign_in_store.hpp"
#include "core/profile_tables.hpp"
#include "core/settings_manager.hpp"

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>

namespace akira {

using libchiaking::unexpected;
using libchiaking::users::AccountContext;
using libchiaking::users::DeviceId;
using libchiaking::users::Region;
namespace auth = libchiaking::users;

SignInStore::SignInStore(SettingsManager& settings)
    : settings(settings)
{
}

auto SignInStore::load_saved_users() -> libchiaking::Expected<std::vector<SavedSignIn>, Error>
{
    std::lock_guard lock(mutex);
    std::vector<SavedSignIn> users;
    users.reserve(rows.size());
    for (const SavedSignIn& row : rows)
        users.push_back(row.clone());
    return users;
}

auto SignInStore::load_home_users() -> libchiaking::Expected<std::vector<SavedHomeUser>, Error>
{
    std::vector<SavedHomeUser> users;
    for (const Profile& profile : settings.profilesSnapshot()) {
        if (!profile.legacy || profile.onlineId.empty())
            continue;
        const auto account = libchiaking::AccountId::from_base64(profile.accountId);
        if (!account)
            continue;
        users.push_back(SavedHomeUser{*account, profile.onlineId});
    }
    return users;
}

auto SignInStore::save(const SavedSignIn& user) -> libchiaking::Expected<void, Error>
{
    {
        std::lock_guard lock(mutex);
        const auto existing = std::ranges::find(rows, user.account, &SavedSignIn::account);
        if (existing != rows.end())
            *existing = user.clone();
        else
            rows.push_back(user.clone());
    }
    return persist();
}

auto SignInStore::save(const SavedHomeUser& user) -> libchiaking::Expected<void, Error>
{
    static_cast<void>(user);
    return {};
}

auto SignInStore::remove(libchiaking::AccountId account) -> libchiaking::Expected<void, Error>
{
    {
        std::lock_guard lock(mutex);
        std::erase_if(rows, [account](const SavedSignIn& row) { return row.account == account; });
    }
    return persist();
}

void SignInStore::read(const toml::table& config)
{
    std::vector<SavedSignIn> loaded;
    ProfileTables::each(config, [&loaded](libchiaking::AccountId account, const toml::table& profile) {
        const auto* row = profile["sign_in"].as_table();
        if (!row)
            return;
        if (auto user = signInOf(*row, account, profile["online_id"].value_or(std::string{})))
            loaded.push_back(std::move(*user));
    });
    std::lock_guard lock(mutex);
    rows = std::move(loaded);
}

void SignInStore::write(toml::table& config) const
{
    std::lock_guard lock(mutex);
    ProfileTables::each(config, [this](libchiaking::AccountId account, toml::table& profile) {
        const auto owned = std::ranges::find(rows, account, &SavedSignIn::account);
        if (owned != rows.end())
            profile.insert_or_assign("sign_in", rowOf(*owned));
    });
}

auto SignInStore::persist() -> libchiaking::Expected<void, Error>
{
    if (settings.writeStores() != 0)
        return unexpected(Error::unavailable);
    return {};
}

auto SignInStore::timeOf(const toml::table& row, std::string_view key) -> std::optional<Time>
{
    const auto milliseconds = row[key].value<int64_t>();
    if (!milliseconds)
        return std::nullopt;
    return Time{std::chrono::duration_cast<Time::duration>(std::chrono::milliseconds{*milliseconds})};
}

void SignInStore::putTime(toml::table& row, std::string_view key, const std::optional<Time>& time)
{
    if (!time)
        return;
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(time->time_since_epoch());
    row.insert(key, static_cast<int64_t>(milliseconds.count()));
}

auto SignInStore::textOf(const toml::table& row, std::string_view key) -> std::optional<std::string>
{
    return row[key].value<std::string>();
}

void SignInStore::putText(toml::table& row, std::string_view key, const std::optional<std::string>& text)
{
    if (text)
        row.insert(key, *text);
}

template <class Token>
auto SignInStore::tokenOf(const toml::table& row, std::string_view key) -> std::optional<Token>
{
    const auto* text = row[key].as_string();
    if (!text || text->get().empty())
        return std::nullopt;
    return Token{std::string_view{text->get()}};
}

auto SignInStore::signInOf(const toml::table& row, libchiaking::AccountId account, std::string onlineId)
    -> std::optional<SavedSignIn>
{
    auto npsso = tokenOf<auth::Npsso>(row, "npsso");
    if (!npsso)
        return std::nullopt;

    SavedSignIn user{account, std::move(onlineId), std::move(*npsso)};
    user.npsso_obtained_at = timeOf(row, "npsso_obtained_at");
    user.npsso_expires_at = timeOf(row, "npsso_expires_at");

    if (auto token = tokenOf<auth::AppToken>(row, "app_token"))
        user.store.emplace(std::move(*token), timeOf(row, "app_token_expires_at").value_or(Time{}),
                           timeOf(row, "app_token_obtained_at"));
    if (auto token = tokenOf<auth::AppRefreshToken>(row, "app_refresh_token"))
        user.store_refresh.emplace(std::move(*token), timeOf(row, "app_refresh_token_obtained_at"),
                                   timeOf(row, "app_refresh_token_expires_at"));
    if (auto token = tokenOf<auth::BearerToken>(row, "remote_play_token"))
        user.remote_play.emplace(std::move(*token), timeOf(row, "remote_play_token_expires_at").value_or(Time{}),
                                 timeOf(row, "remote_play_token_obtained_at"));
    if (auto token = tokenOf<auth::RemotePlayRefreshToken>(row, "remote_play_refresh_token"))
        user.remote_play_refresh.emplace(std::move(*token), timeOf(row, "remote_play_refresh_token_obtained_at"),
                                         timeOf(row, "remote_play_refresh_token_expires_at"));

    const auto country = textOf(row, "region_country");
    const auto language = textOf(row, "region_language");
    if (country && language)
        user.region = Region{*country, *language};

    AccountContext::Stated stated;
    stated.legal_country = textOf(row, "legal_country");
    stated.locale = textOf(row, "locale");
    stated.age = row["age"].value<int64_t>();
    stated.is_child = row["is_child"].value<bool>();
    stated.issued_at = timeOf(row, "context_issued_at");
    user.context = AccountContext::stating(stated);

    if (const auto* device = row["device_id"].as_string()) {
        if (auto parsed = DeviceId::parse(device->get()))
            user.device.emplace(std::move(*parsed));
    }
    return user;
}

auto SignInStore::rowOf(const SavedSignIn& user) -> toml::table
{
    toml::table row;
    row.insert("npsso", std::string{user.npsso.view()});
    putTime(row, "npsso_obtained_at", user.npsso_obtained_at);
    putTime(row, "npsso_expires_at", user.npsso_expires_at);

    if (user.store) {
        row.insert("app_token", std::string{user.store->token.view()});
        putTime(row, "app_token_expires_at", user.store->expires_at);
        putTime(row, "app_token_obtained_at", user.store->obtained_at);
    }
    if (user.store_refresh) {
        row.insert("app_refresh_token", std::string{user.store_refresh->token.view()});
        putTime(row, "app_refresh_token_obtained_at", user.store_refresh->obtained_at);
        putTime(row, "app_refresh_token_expires_at", user.store_refresh->expires_at);
    }
    if (user.remote_play) {
        row.insert("remote_play_token", std::string{user.remote_play->token.view()});
        putTime(row, "remote_play_token_expires_at", user.remote_play->expires_at);
        putTime(row, "remote_play_token_obtained_at", user.remote_play->obtained_at);
    }
    if (user.remote_play_refresh) {
        row.insert("remote_play_refresh_token", std::string{user.remote_play_refresh->token.view()});
        putTime(row, "remote_play_refresh_token_obtained_at", user.remote_play_refresh->obtained_at);
        putTime(row, "remote_play_refresh_token_expires_at", user.remote_play_refresh->expires_at);
    }
    if (user.region) {
        row.insert("region_country", user.region->country);
        row.insert("region_language", user.region->language);
    }

    putText(row, "legal_country", user.context.legal_country());
    putText(row, "locale", user.context.locale());
    if (const auto age = user.context.age())
        row.insert("age", static_cast<int64_t>(*age));
    if (const auto child = user.context.is_child())
        row.insert("is_child", *child);
    putTime(row, "context_issued_at", user.context.issued_at());

    if (user.device)
        row.insert("device_id", std::string{user.device->view()});
    return row;
}

}
