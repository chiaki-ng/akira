#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <libchiaking/consoles.hpp>
#include <libchiaking/platform/platform.hpp>

#include "accounts/accounts.hpp"
#include "consoles/console_actions.hpp"
#include "consoles/console_list.hpp"
#include "consoles/register_flow.hpp"
#include "consoles/ui/console_commands.hpp"
#include "consoles/ui/console_starter.hpp"
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
    using SetUp = std::function<void()>;

    ConsoleOperations(app::Services& services, ConsoleShelf& shelf, RecentShelf& recent, app::Notifier& notifier,
        ConsoleStarter& starter, SetUp setUp);
    ~ConsoleOperations() override;

    ConsoleOperations(const ConsoleOperations&) = delete;
    ConsoleOperations& operator=(const ConsoleOperations&) = delete;

    void follow();
    void refresh();
    void lookUpAccount();
    void streamEnded();

    void primary(const std::string& catalogName) override;

    void chooseRoute(const std::string& catalogName, ConsoleEntry::Route route) override;
    void registerOverInternet(const std::string& catalogName) override;
    void askPin(const std::string& catalogName) override;
    void registerWithPin(const std::string& catalogName, const std::string& pin) override;
    void cancelRegistration(const std::string& catalogName) override;
    void link(const std::string& catalogName, const std::string& localName) override;
    void edit(const std::string& catalogName) override;
    void saveEdit(const std::string& catalogName, const ConsoleShelf::Edit& values) override;
    void askDelete(const std::string& catalogName) override;
    void remove(const std::string& catalogName) override;
    void openAdd() override;
    void chooseAddMode(ConsoleShelf::AddMode mode) override;
    void addByAddress(const std::string& address, bool ps5) override;
    void findRemote() override;
    void setUpAccount() override;
    void launchTitle(const std::string& catalogName, const std::string& titleId) override;

private:
    using Clock = std::chrono::steady_clock;
    using Installed = std::vector<libchiaking::consoles::InstalledTitle>;

    static constexpr std::chrono::minutes recentFreshFor{ 10 };
    static constexpr std::size_t galleryLimit = 8;
    static constexpr int backgroundWidth = 1920;
    static constexpr int heroWidth = 1440;
    static constexpr int logoWidth = 1024;

    void refreshRecent();
    void showRecent(const std::string& catalogName, Installed titles);
    void loadBanner(const std::string& catalogName);
    void loadArt(const std::string& catalogName, const RecentPlay::Title& chosen, const accounts::Accounts::User& user,
        bool latest);
    void titleStarted(const std::string& catalogName, const std::string& name,
        libchiaking::Expected<libchiaking::consoles::StartState, libchiaking::consoles::StartFailure> started);
    auto recentHost() const -> std::optional<std::string>;
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
    auto findsRemote() const -> bool;
    void publishScan();
    auto saveConsolePin(Host& target, const std::string& pin) -> std::optional<std::string>;
    auto saveAddress(Host& target, const std::string& address) -> std::optional<std::string>;
    void saveTakion(Host& target, int choice);
    auto takionLabels(Host& target) -> std::vector<std::string>;
    auto refusalText(RegisterFlow::Refusal refusal) const -> std::string;
    auto manualRefusalText(ConsoleActions::ManualRefusal refusal) const -> std::string;

    app::Services& services;
    ConsoleShelf& shelf;
    RecentShelf& recent;
    app::Notifier& notifier;
    ConsoleStarter& starter;
    SetUp setUp;
    Installed installed;
    std::string recentFor;
    std::optional<Clock::time_point> recentUntil;
    libchiaking::platform::StopSource recentStop;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

}
