#pragma once

#include <cstdint>
#include <functional>

namespace akira::settings {
class SettingsStore;
}

namespace akira::accounts {

class AccountSettings;

class ProfileSwitch {
public:
    ProfileSwitch(AccountSettings& accounts, settings::SettingsStore& store, std::function<void()> profileChanged);

    void to(int64_t profileId);

private:
    AccountSettings& accounts;
    settings::SettingsStore& store;
    std::function<void()> profileChanged;
};

}
