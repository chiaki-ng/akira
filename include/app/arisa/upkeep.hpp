#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "app/services.hpp"
#include "platform/updates/update_info.hpp"
#include "settings/maintenance.hpp"

class HttpSession;

namespace akira::app {
class Notifier;
}

namespace akira::app::arisa {

class Upkeep final : public settings::Maintenance {
public:
    Upkeep(Services& services, std::function<void()> refreshConsoles, std::function<void()> stop);
    ~Upkeep() override;

    Upkeep(const Upkeep&) = delete;
    Upkeep& operator=(const Upkeep&) = delete;

    void checkOnLaunch(Notifier& notifier);

    auto legacyProfile() const -> bool override;
    auto localSubnet() const -> std::string override;
    void restartDiscovery() override;
    void flushTrophyCache() override;
    void accountNameDisplayChanged() override;
    void consolesChanged() override;
    void probeTls(std::function<void(std::string)> report) override;
    void checkForUpdate(std::string channel, std::function<void(UpdateCheck)> done) override;
    void simulateUpdate(std::string server, std::function<void(UpdateCheck)> done) override;
    void installUpdate(std::function<void(UpdateProgress)> progress) override;
    void cancelUpdate() override;
    void quit() override;

private:
    static constexpr std::int64_t launchCheckInterval = 86400;

    static void runTlsProbe(HttpSession& session, const std::function<void(std::string)>& show);
    void post(std::function<void()> work);
    void install(const platform::UpdateInfo& info, const std::function<void(UpdateProgress)>& progress);

    Services& services;
    std::function<void()> refreshConsoles;
    std::function<void()> stop;
    std::optional<platform::UpdateInfo> offered;
    std::shared_ptr<std::atomic<bool>> cancelled = std::make_shared<std::atomic<bool>>(false);
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
};

}
