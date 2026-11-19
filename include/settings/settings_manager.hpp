#ifndef AKIRA_SETTINGS_MANAGER_HPP
#define AKIRA_SETTINGS_MANAGER_HPP

#include "accounts/account_settings.hpp"
#include "app/app_settings.hpp"
#include "cloud/cloud_settings.hpp"
#include "consoles/console_settings.hpp"
#include "consoles/host_catalog.hpp"
#include "network/network_settings.hpp"
#include "play/controller_settings.hpp"
#include "play/stream_settings.hpp"
#include "play/stream_state.hpp"
#include "settings/developer_settings.hpp"
#include "settings/settings_store.hpp"

namespace akira::settings {

class SettingsManager {
public:
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    static SettingsManager* getInstance();

    SettingsStore& store() { return settingsStore; }
    play::StreamSettings& stream() { return streamSettings; }
    play::ControllerSettings& controllers() { return controllerSettings; }
    cloud::CloudSettings& cloud() { return cloudSettings; }
    network::NetworkSettings& network() { return networkSettings; }
    consoles::ConsoleSettings& consoles() { return consoleSettings; }
    accounts::AccountSettings& accounts() { return accountSettings; }
    app::AppSettings& app() { return appSettings; }
    DeveloperSettings& developer() { return developerSettings; }
    consoles::HostCatalog& hosts() { return hostCatalog; }
    play::StreamState& streamState() { return state; }

private:
    SettingsManager();

    SettingsStore settingsStore;
    DeveloperSettings developerSettings;
    play::StreamSettings streamSettings;
    play::ControllerSettings controllerSettings;
    cloud::CloudSettings cloudSettings;
    network::NetworkSettings networkSettings;
    accounts::AccountSettings accountSettings;
    consoles::ConsoleSettings consoleSettings;
    app::AppSettings appSettings;
    consoles::HostCatalog hostCatalog;
    play::StreamState state;
};

}

#endif // AKIRA_SETTINGS_MANAGER_HPP
