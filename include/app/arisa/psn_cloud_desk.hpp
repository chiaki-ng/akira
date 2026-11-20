#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <libchiaking/platform/platform.hpp>

#include "app/services.hpp"
#include "cloud/cloud_desk.hpp"

namespace akira::consoles {
class Host;
}

namespace akira::app::arisa {

class PsnCloudDesk final : public cloud::CloudDesk {
public:
    using Play = std::function<void(std::shared_ptr<consoles::Host>)>;
    using SignIn = std::function<void()>;

    PsnCloudDesk(Services& services, Play play, SignIn signIn);
    ~PsnCloudDesk() override;

    PsnCloudDesk(const PsnCloudDesk&) = delete;
    PsnCloudDesk& operator=(const PsnCloudDesk&) = delete;

    auto snapshot() const -> cloud::Snapshot override;
    auto snapshotOf(const cloud::FetchFailure& failure) const -> cloud::Snapshot override;
    void refresh(bool force, Snapshotted done, Fetched fetched) override;
    void clearCatalogCache() override;

    void details(const cloud::Game& game, Detailed done) override;
    void checkStorefront(const cloud::Game& game, Checked done) override;
    void refreshTitle(const cloud::Game& game, Refreshed done) override;
    void clearForLaunch(const cloud::Game& game, Cleared cleared, Refused refused, Observed observed) override;
    void cancelLaunch() override;
    void launch(std::shared_ptr<cloud::CloudLaunch> cleared) override;
    void signIn() override;

    auto signedIn() const -> bool override;
    auto nowMs() const -> std::int64_t override;

    auto shortcuts() const -> std::vector<cloud::Game> override;
    void setShortcuts(std::vector<cloud::Game> games) override;
    auto filter() const -> std::string override;
    void setFilter(std::string key) override;
    auto sortState() const -> int override;
    void setSortState(int state) override;

private:
    Services& services;
    Play play;
    SignIn openSignIn;
    libchiaking::platform::StopSource lifetime;
    libchiaking::platform::StopSource launching;
};

}
