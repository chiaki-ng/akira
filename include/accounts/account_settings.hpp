#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "accounts/profile.hpp"
#include "accounts/sign_in_store.hpp"
#include "cloud/models.hpp"
#include "settings/section.hpp"

namespace akira::settings {
class SettingsStore;
}

namespace akira::accounts {

class AccountSettings final : public settings::Section {
public:
    using LegacyGate = std::function<void(bool)>;

    AccountSettings(settings::SettingsStore& store, LegacyGate legacyGate);
    ~AccountSettings() override;

    AccountSettings(const AccountSettings&) = delete;
    AccountSettings& operator=(const AccountSettings&) = delete;

    void write(toml::table& config) const override;

    const std::vector<Profile>& profiles() const { return entries; }
    std::vector<Profile> profilesSnapshot() const;
    Profile* findProfile(int64_t id);
    const Profile* findProfile(int64_t id) const;
    Profile* activeProfile();
    const Profile* activeProfile() const;
    Profile* ensureActiveProfile();
    int64_t activeProfileId() const { return activeId; }
    void setActiveProfileId(int64_t id);
    void logProfileCensus(const char* stage) const;
    bool readProfile(int64_t id, const std::function<void(const Profile&)>& fn) const;
    bool updateProfile(int64_t id, const std::function<void(Profile&)>& fn);
    bool activeProfileTrophiesEnabled() const;
    void setActiveProfileTrophiesEnabled(bool enabled);
    int64_t addProfile(const Profile& profile);
    void removeProfile(int64_t id);

    /*
     * Legacy is a property of the active profile, not of the install: a household can
     * hold one of each. Everything PSN already reads through getActiveProfile(), so this
     * is the grain the rest of the code already has.
     */
    bool isLegacyProfileActive() const;

    /*
     * Mirrors isLegacyProfileActive() into the HTTP layer. Must be called after anything
     * that changes which profile is active or what credentials it holds - not only on a
     * profile switch - or the gate goes stale in either direction.
     */
    void refreshLegacyGate() const;

    std::string psnOnlineId() const;
    void setPsnOnlineId(const std::string& id);
    std::string psnAccountId() const;
    void setPsnAccountId(const std::string& id);

    const std::vector<cloud::Game>& cloudShortcuts() const;
    void setCloudShortcuts(std::vector<cloud::Game> shortcuts);

    SignInStore& signIns() { return signInStore; }

private:
    static std::vector<cloud::Game> readShortcuts(const toml::array* rows);
    static toml::array shortcutRows(const std::vector<cloud::Game>& shortcuts);

    void read(const toml::table& config);

    settings::SettingsStore& store;
    LegacyGate legacyGate;
    std::vector<Profile> entries;
    int64_t activeId = 0;
    int64_t nextId = 1;
    mutable std::recursive_mutex mutex;
    SignInStore signInStore;
};

}
