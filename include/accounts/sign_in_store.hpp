#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <libchiaking/users.hpp>
#include <toml++/toml.hpp>

#include "settings/section.hpp"

namespace akira::settings {
class SettingsStore;
}

namespace akira::accounts {

class AccountSettings;

class SignInStore final : public libchiaking::users::CredentialStore, public settings::Section {
public:
    using SavedSignIn = libchiaking::users::SavedSignIn;
    using SavedHomeUser = libchiaking::users::SavedHomeUser;

    SignInStore(const AccountSettings& accounts, settings::SettingsStore& store);
    ~SignInStore() override;

    SignInStore(const SignInStore&) = delete;
    SignInStore& operator=(const SignInStore&) = delete;

    [[nodiscard]] auto load_saved_users()
        -> libchiaking::Expected<std::vector<SavedSignIn>, Error> override;
    [[nodiscard]] auto load_home_users()
        -> libchiaking::Expected<std::vector<SavedHomeUser>, Error> override;
    [[nodiscard]] auto save(const SavedSignIn& user) -> libchiaking::Expected<void, Error> override;
    [[nodiscard]] auto save(const SavedHomeUser& user) -> libchiaking::Expected<void, Error> override;
    [[nodiscard]] auto remove(libchiaking::AccountId account)
        -> libchiaking::Expected<void, Error> override;

    void write(toml::table& config) const override;

private:
    using Time = SavedSignIn::Time;

    static auto timeOf(const toml::table& row, std::string_view key) -> std::optional<Time>;
    static void putTime(toml::table& row, std::string_view key, const std::optional<Time>& time);
    static void putText(toml::table& row, std::string_view key, const std::optional<std::string>& text);
    static auto textOf(const toml::table& row, std::string_view key) -> std::optional<std::string>;
    template <class Token>
    static auto tokenOf(const toml::table& row, std::string_view key) -> std::optional<Token>;
    static auto signInOf(const toml::table& row, libchiaking::AccountId account, std::string onlineId)
        -> std::optional<SavedSignIn>;
    static auto rowOf(const SavedSignIn& user) -> toml::table;

    void read(const toml::table& config);
    auto persist() -> libchiaking::Expected<void, Error>;

    const AccountSettings& accounts;
    settings::SettingsStore& store;
    mutable std::mutex mutex;
    std::vector<SavedSignIn> rows;
};

}
