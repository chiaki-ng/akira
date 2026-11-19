#include "consoles/console_actions.hpp"

#include <cstdlib>
#include <utility>

#include "accounts/account_settings.hpp"
#include "consoles/console_address.hpp"
#include "consoles/console_entry.hpp"
#include "consoles/console_pin.hpp"
#include "consoles/host_catalog.hpp"
#include "log/log.hpp"
#include "play/chiaki_core.hpp"
#include "settings/developer_settings.hpp"
#include "settings/settings_store.hpp"

namespace akira::consoles {

ConsoleActions::ConsoleActions(HostCatalog& hosts, settings::SettingsStore& store,
                               const accounts::AccountSettings& accounts, const settings::DeveloperSettings& developer,
                               network::WireGuardManager& wireGuard, play::ChiakiCore& core)
    : hosts(hosts)
    , store(store)
    , accounts(accounts)
    , developer(developer)
    , wireGuard(wireGuard)
    , core(core)
{
}

std::optional<PsnIdentity::Missing> ConsoleActions::missingIdentity(const Host& host) const
{
    return PsnIdentity(accounts.psnAccountId(), accounts.psnOnlineId()).missingFor(host.needsAccountId());
}

bool ConsoleActions::wake(Host& host)
{
    log::info("Wake {}", host.getHostName());
    return static_cast<bool>(host.wake(wireGuard));
}

HostRegisterError ConsoleActions::registerWithPin(Host& host, const std::string& pin, Host::PairingCallback done)
{
    return host.registerHost(std::atoi(pin.c_str()), keepingPairing(host, std::move(done)));
}

HostRegisterError ConsoleActions::registerOverInternet(Host& host, Host::PairingStageCallback stage,
                                                       Host::PairingCallback done)
{
    return host.registerOverInternet(std::move(stage), keepingPairing(host, std::move(done)));
}

void ConsoleActions::cancelRegistration(Host& host)
{
    host.cancelRegistration();
}

Host::PairingCallback ConsoleActions::keepingPairing(Host& host, Host::PairingCallback done)
{
    return [target = &host, saved = &store, done = std::move(done)](Host::PairingOutcome outcome) {
        if (outcome == Host::PairingOutcome::Paired) {
            target->setNeedsLink(false);
            saved->save();
        }
        done(outcome);
    };
}

std::expected<std::string, ConsoleActions::LinkRefusal> ConsoleActions::link(Host& remote, const std::string& localName)
{
    const auto found = hosts.entries().find(localName);
    if (found == hosts.entries().end() || !found->second)
        return std::unexpected(LinkRefusal::NoLocal);
    Host* local = found->second.get();

    const std::string remoteName = ConsoleEntry(remote.facts(), false).displayName();
    if (auto renamed = hosts.rename(localName, remoteName); !renamed)
        return std::unexpected(renamed.error() == HostCatalog::RenameRefusal::NameTaken ? LinkRefusal::NameTaken
                                                                                         : LinkRefusal::NoLocal);
    remote.copyRegistrationFrom(local);
    remote.setNeedsLink(false);
    local->setRemoteDuid(remote.getRemoteDuid());
    store.save();
    return remoteName;
}

bool ConsoleActions::setConsolePin(Host& host, std::string text)
{
    const auto pin = ConsolePin::parse(std::move(text));
    if (!pin)
        return false;
    host.setConsolePIN(pin->text());
    store.save();
    return true;
}

std::expected<ConsoleActions::AddressChange, Host::AddressRefusal> ConsoleActions::moveTo(Host& host,
                                                                                         const std::string& text)
{
    if (text.empty() || text == host.getHostAddr())
        return AddressChange::Unchanged;
    if (!ConsoleAddress::parse(text))
        return std::unexpected(Host::AddressRefusal::Invalid);
    if (auto moved = host.moveTo(text); !moved)
        return std::unexpected(moved.error());
    store.save();
    return AddressChange::Moved;
}

TakionPinChoice ConsoleActions::takionPins(const Host& host) const
{
    return TakionPinChoice(host.pinnableTakionVersions(), host.getPinnedTakionVersion(),
                           developer.takionVersion(host.isPS5() ? settings::TakionTarget::PS5
                                                                : settings::TakionTarget::PS4));
}

void ConsoleActions::pinTakion(Host& host, std::optional<std::uint32_t> version)
{
    host.pinTakionVersion(version);
    store.save();
}

void ConsoleActions::remove(const std::string& name)
{
    auto* signedIn = core.accounts();
    auto hostIt = hosts.entries().find(name);
    if (signedIn && hostIt != hosts.entries().end() && hostIt->second->getConsole()) {
        if (const auto user = signedIn->activeUser()) {
            if (!core.chiaki().consoles().unpair(user->account_id(), *hostIt->second->getConsole()))
                log::error("Could not forget the pairing with {}", name);
        }
    }
    hosts.removeActiveProfileRegistration(name);
    store.save();
}

std::expected<void, ConsoleActions::ManualRefusal> ConsoleActions::addManual(
    const std::string& name, const std::string& address, libchiaking::consoles::Generation generation)
{
    if (name.empty())
        return std::unexpected(ManualRefusal::NoName);
    if (address.empty())
        return std::unexpected(ManualRefusal::NoAddress);
    if (!ConsoleAddress::parse(address))
        return std::unexpected(ManualRefusal::InvalidAddress);
    if (hosts.entries().contains(name))
        return std::unexpected(ManualRefusal::DuplicateName);

    Host* host = hosts.getOrCreate(name);
    host->setHostType(HostType::Manual);
    host->setHostAddr(address);
    host->setDiscovered(true);
    host->setFirmware(Host::Firmware::newest(generation));
    store.save();

    log::info("Added host: {} at {}", name, address);
    return {};
}

}
