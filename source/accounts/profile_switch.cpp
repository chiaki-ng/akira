#include "accounts/profile_switch.hpp"

#include <utility>

#include "accounts/account_settings.hpp"
#include "settings/settings_store.hpp"

namespace akira::accounts {

ProfileSwitch::ProfileSwitch(AccountSettings& accounts, settings::SettingsStore& store,
                             std::function<void()> profileChanged)
    : accounts(accounts)
    , store(store)
    , profileChanged(std::move(profileChanged))
{
}

void ProfileSwitch::to(int64_t profileId)
{
    accounts.setActiveProfileId(profileId);
    store.save();
    if (profileChanged)
        profileChanged();
}

}
