#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "consoles/console_facts.hpp"
#include "consoles/host.hpp"

namespace akira::accounts {
class AccountSettings;
}

namespace akira::app {
class AppSettings;
}

namespace akira::settings {
class SettingsStore;
}

namespace akira::consoles {

class ConsoleSettings;

class HostCatalog {
public:
    using Hosts = std::map<std::string, std::unique_ptr<Host>>;

    HostCatalog(ConsoleSettings& consoles, accounts::AccountSettings& accounts, const app::AppSettings& app,
                settings::SettingsStore& store, app::UiDispatcher& dispatcher);
    ~HostCatalog();

    HostCatalog(const HostCatalog&) = delete;
    HostCatalog& operator=(const HostCatalog&) = delete;

    Hosts& entries() { return hosts; }
    std::vector<ConsoleFacts> facts() const;
    Host* getOrCreate(const std::string& name);
    Host* findByDuid(const std::string& duid);
    void remove(const std::string& name);
    void rename(const std::string& oldName, const std::string& newName);
    void removeActiveProfileRegistration(const std::string& name);
    void forgetProfile(int64_t profileId);
    bool adoptLegacyRegistrations(Host& host);

private:
    std::vector<HostRecord> persisted();

    ConsoleSettings& consoles;
    accounts::AccountSettings& accounts;
    const app::AppSettings& app;
    settings::SettingsStore& store;
    app::UiDispatcher& dispatcher;
    Hosts hosts;
};

}
