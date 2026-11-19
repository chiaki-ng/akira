#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>

#include <libchiaking/consoles.hpp>

#include "consoles/host.hpp"
#include "consoles/psn_identity.hpp"
#include "consoles/takion_pin_choice.hpp"

namespace akira::accounts {
class AccountSettings;
}

namespace akira::network {
class WireGuardManager;
}

namespace akira::play {
class ChiakiCore;
}

namespace akira::settings {
class DeveloperSettings;
class SettingsStore;
}

namespace akira::consoles {

class HostCatalog;

class ConsoleActions {
public:
    enum class AddressChange { Unchanged, Moved };
    enum class ManualRefusal { NoName, NoAddress, InvalidAddress, DuplicateName };

    ConsoleActions(HostCatalog& hosts, settings::SettingsStore& store, const accounts::AccountSettings& accounts,
                   const settings::DeveloperSettings& developer, network::WireGuardManager& wireGuard,
                   play::ChiakiCore& core);

    std::optional<PsnIdentity::Missing> missingIdentity(const Host& host) const;
    [[nodiscard]] bool wake(Host& host);
    [[nodiscard]] HostRegisterError registerWithPin(Host& host, const std::string& pin, Host::PairingCallback done);
    [[nodiscard]] HostRegisterError registerOverInternet(Host& host, Host::PairingStageCallback stage,
                                                         Host::PairingCallback done);
    void cancelRegistration(Host& host);
    std::optional<std::string> link(Host& remote, const std::string& localName);
    [[nodiscard]] bool setConsolePin(Host& host, std::string text);
    [[nodiscard]] std::expected<AddressChange, Host::AddressRefusal> moveTo(Host& host, const std::string& text);
    TakionPinChoice takionPins(const Host& host) const;
    void pinTakion(Host& host, std::optional<std::uint32_t> version);
    void remove(const std::string& name);
    [[nodiscard]] std::expected<void, ManualRefusal> addManual(const std::string& name, const std::string& address,
                                                               libchiaking::consoles::Generation generation);

private:
    Host::PairingCallback keepingPairing(Host& host, Host::PairingCallback done);

    HostCatalog& hosts;
    settings::SettingsStore& store;
    const accounts::AccountSettings& accounts;
    const settings::DeveloperSettings& developer;
    network::WireGuardManager& wireGuard;
    play::ChiakiCore& core;
};

}
