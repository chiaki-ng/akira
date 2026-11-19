#include "settings/settings_manager.hpp"
#include "network/custom_dns.hpp"
#include "platform/factory.hpp"
#include "util/http.hpp"

namespace akira::settings {

SettingsManager::SettingsManager()
    : settingsStore("sdmc:/switch/akira"),
      developerSettings(settingsStore),
      streamSettings(settingsStore,
          platform::PlatformFactory::instance().supplies(platform::PlatformCapability::fsr_4k)),
      controllerSettings(settingsStore),
      cloudSettings(settingsStore),
      networkSettings(settingsStore,
          [](const std::vector<network::Ipv4Address>& servers) { network::CustomDns::instance().configure(servers); }),
      accountSettings(settingsStore, [](bool legacy) { httpSetSonyBlocked(legacy); }),
      consoleSettings(settingsStore),
      appSettings(settingsStore),
      hostCatalog(consoleSettings, accountSettings, appSettings, settingsStore),
      state(streamSettings, cloudSettings, developerSettings)
{
    settingsStore.finishLoading();
}

SettingsManager* SettingsManager::getInstance()
{
    static SettingsManager* instance = new SettingsManager();
    return instance;
}

}
