#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <libchiaking/consoles.hpp>
#include <libchiaking/platform/platform.hpp>

#include "consoles/console_actions.hpp"
#include "consoles/console_list.hpp"
#include "consoles/register_flow.hpp"
#include "consoles/ui/console_commands.hpp"
#include "consoles/ui/console_shelf.hpp"
#include "consoles/ui/recent_shelf.hpp"

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
    using SetUp = std::function<void()>;

    ConsoleOperations(app::Services& services, ConsoleShelf& shelf, RecentShelf& recent, app::Notifier& notifier,
        Connect connect, SetUp setUp);
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
    void setUpAccount() override;
    void launchTitle(const std::string& catalogName, const std::string& titleId) override;

private:
    using Clock = std::chrono::steady_clock;
    using Installed = std::vector<libchiaking::consoles::InstalledTitle>;

    static constexpr std::chrono::minutes recentFreshFor{ 10 };

    void refreshRecent();
    void showRecent(const std::string& catalogName, Installed titles);
    void titleStarted(const std::string& catalogName, const std::string& name,
        libchiaking::Expected<libchiaking::consoles::StartState, libchiaking::consoles::StartFailure> started);
    auto recentHost() const -> Host*;
    static auto played(const Installed& titles) -> std::vector<RecentPlay::Title>;
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
    RecentShelf& recent;
    app::Notifier& notifier;
    Connect connect;
    SetUp setUp;
    Installed installed;
    std::string recentFor;
    std::optional<Clock::time_point> recentUntil;
    libchiaking::platform::StopSource recentStop;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

}
