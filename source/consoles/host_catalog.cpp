#include "consoles/host_catalog.hpp"
#include "consoles/console_settings.hpp"
#include "accounts/account_settings.hpp"
#include "settings/settings_store.hpp"
#include "log/log.hpp"

#include <utility>

namespace akira::consoles {

HostCatalog::HostCatalog(ConsoleSettings& consoles, accounts::AccountSettings& accounts, const app::AppSettings& app,
                         settings::SettingsStore& store, app::UiDispatcher& dispatcher)
    : consoles(consoles), accounts(accounts), app(app), store(store), dispatcher(dispatcher)
{
    for (auto& [name, record] : consoles.follow([this] { return persisted(); })) {
        auto host = std::make_unique<Host>(std::move(record), accounts, app, dispatcher);
        host->inConfig = true;
        hosts.insert_or_assign(name, std::move(host));
    }
}

HostCatalog::~HostCatalog()
{
    consoles.unfollow(persisted());
}

std::vector<HostRecord> HostCatalog::persisted()
{
    std::vector<HostRecord> records;
    records.reserve(hosts.size());
    for (const auto& [name, host] : hosts) {
        host->inConfig = true;
        HostRecord record = host->record();
        record.nickname = name;
        records.push_back(std::move(record));
    }
    return records;
}

std::vector<ConsoleFacts> HostCatalog::facts() const
{
    std::vector<ConsoleFacts> known;
    known.reserve(hosts.size());
    for (const auto& [name, host] : hosts) {
        if (!host) {
            log::error("Null host in hosts map for key: {}", name);
            continue;
        }
        ConsoleFacts facts = host->facts();
        facts.catalogName = name;
        known.push_back(std::move(facts));
    }
    return known;
}

Host* HostCatalog::getOrCreate(const std::string& name)
{
    auto [entry, created] = hosts.try_emplace(name);
    if (created)
        entry->second = std::make_unique<Host>(HostRecord{.nickname = name}, accounts, app, dispatcher);

    Host* host = entry->second.get();
    if (created && host->consoleId == 0)
        host->consoleId = consoles.claimConsoleId();
    return host;
}

void HostCatalog::remove(const std::string& name)
{
    hosts.erase(name);
}

void HostCatalog::removeActiveProfileRegistration(const std::string& name)
{
    auto it = hosts.find(name);
    if (it == hosts.end() || !it->second)
        return;

    it->second->removeRegistrationsForProfile(accounts.activeProfileId());

    if (!it->second->hasRpKey())
        hosts.erase(it);
}

void HostCatalog::forgetProfile(int64_t profileId)
{
    /*
     * Registrations are keyed by profile, so dropping the profile without them left the
     * rp keys behind forever. Deleting and re-pairing is the sanctioned way to change a
     * profile's mode, which makes this the common path rather than a rare one.
     */
    for (auto& [name, host] : hosts) {
        if (host)
            host->removeRegistrationsForProfile(profileId);
    }
}

void HostCatalog::rename(const std::string& oldName, const std::string& newName)
{
    auto it = hosts.find(oldName);
    if (it == hosts.end()) return;

    auto host = std::move(it->second);
    hosts.erase(it);
    host->hostName = newName;
    hosts[newName] = std::move(host);
}

Host* HostCatalog::findByDuid(const std::string& duid)
{
    if (duid.empty())
        return nullptr;
    for (auto& [name, host] : hosts) {
        if (host && host->getRemoteDuid() == duid)
            return host.get();
    }
    return nullptr;
}

bool HostCatalog::adoptLegacyRegistrations(Host& host)
{
    namespace chiaki = libchiaking::consoles;
    if (!host.hardwareId || host.legacyRegistrations.empty())
        return false;
    const auto firmware = host.getFirmware();
    if (!firmware)
        return false;

    const std::string name = host.serverNickname.empty() ? host.hostName : host.serverNickname;
    chiaki::SavedConsole console{*host.hardwareId, *firmware,
        chiaki::ConsoleName::create(name, chiaki::ConsoleName::Source::pairing)
            .value_or(chiaki::ConsoleName::of(firmware->generation()))};
    if (auto address = chiaki::Host::create(host.getHostAddr(), firmware->generation()))
        console.last_address = std::move(address.value());

    std::vector<chiaki::SavedPairing> adopted;
    for (const Registration& reg : host.legacyRegistrations) {
        std::optional<libchiaking::AccountId> account;
        accounts.readProfile(reg.profileId, [&account](const accounts::Profile& profile) {
            account = libchiaking::AccountId::from_base64(profile.accountId);
        });
        if (!account)
            continue;
        adopted.push_back(chiaki::SavedPairing{*account, *host.hardwareId, reg.keys.clone(),
            chiaki::DeviceUid::parse(host.remoteDuid)});
    }
    if (adopted.empty())
        return false;

    if (!consoles.pairings().adopt(std::move(console), std::move(adopted))) {
        log::error("Could not adopt the saved registrations of {}", host.hostName);
        return false;
    }
    host.legacyRegistrations.clear();
    store.save();
    log::info("Adopted the saved registrations of {} into the core's pairings", host.hostName);
    return true;
}

}
