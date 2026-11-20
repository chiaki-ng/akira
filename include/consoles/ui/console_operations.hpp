#pragma once

#include <functional>
#include <string>

#include "consoles/console_actions.hpp"
#include "consoles/console_list.hpp"
#include "consoles/register_flow.hpp"
#include "consoles/ui/console_commands.hpp"
#include "consoles/ui/console_shelf.hpp"

namespace akira::app {
class Notifier;
struct Services;
}

namespace akira::consoles {
class Host;
}

namespace akira::consoles::ui {

class ConsoleOperations final : public ConsoleCommands {
public:
    using Connect = std::function<void(Host&)>;

    ConsoleOperations(app::Services& services, ConsoleShelf& shelf, app::Notifier& notifier, Connect connect);
    ~ConsoleOperations() override;

    ConsoleOperations(const ConsoleOperations&) = delete;
    ConsoleOperations& operator=(const ConsoleOperations&) = delete;

    void follow();
    void refresh();
    void streamEnded();

    void primary(const std::string& catalogName) override;
    void option(const std::string& catalogName, ConsoleEntry::Option option) override;
    void wake(const std::string& catalogName) override;
    void registerOverInternet(const std::string& catalogName) override;
    void askPin(const std::string& catalogName) override;
    void registerWithPin(const std::string& catalogName, const std::string& pin) override;
    void cancelRegistration(const std::string& catalogName) override;
    void link(const std::string& catalogName, const std::string& localName) override;
    void setConsolePin(const std::string& catalogName, const std::string& pin) override;
    void setAddress(const std::string& catalogName, const std::string& address) override;
    void pinTakion(const std::string& catalogName, int choice) override;
    void remove(const std::string& catalogName) override;
    void addManual(const std::string& name, const std::string& address, bool ps5) override;
    void findRemote() override;
    auto findsRemote() const -> bool override;

private:
    auto host(const std::string& catalogName) const -> Host*;
    auto actions() const -> ConsoleActions;
    auto list() const -> ConsoleList;
    auto identityMissing(const Host& host, bool registering) -> bool;
    void play(Host& host);
    void beginRegistration(Host& host);
    void offerLink(Host& host);
    void pinEnded(const std::string& catalogName, Host::PairingOutcome outcome);
    void internetEnded(const std::string& catalogName, Host::PairingOutcome outcome);
    void internetStage(const std::string& catalogName, int index, int total);
    auto refusalText(RegisterFlow::Refusal refusal) const -> std::string;
    auto manualRefusalText(ConsoleActions::ManualRefusal refusal) const -> std::string;

    app::Services& services;
    ConsoleShelf& shelf;
    app::Notifier& notifier;
    Connect connect;
};

}
