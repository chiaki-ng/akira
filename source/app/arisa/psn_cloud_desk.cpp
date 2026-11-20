#include "app/arisa/psn_cloud_desk.hpp"

#include <chrono>
#include <utility>

#include "accounts/account_settings.hpp"
#include "app/app_settings.hpp"
#include "app/ui_dispatcher.hpp"
#include "cloud/cloud_launch.hpp"
#include "cloud/cloud_settings.hpp"
#include "cloud/service.hpp"
#include "consoles/host.hpp"
#include "consoles/host_record.hpp"
#include "play/chiaki_core.hpp"
#include "settings/settings_store.hpp"

namespace akira::app::arisa {

PsnCloudDesk::PsnCloudDesk(Services& services, Play play, SignIn signIn)
    : services(services)
    , play(std::move(play))
    , openSignIn(std::move(signIn))
{
}

PsnCloudDesk::~PsnCloudDesk()
{
    static_cast<void>(lifetime.request_stop());
    static_cast<void>(launching.request_stop());
}

auto PsnCloudDesk::snapshot() const -> cloud::Snapshot
{
    return services.cloud.snapshotForActiveProfile();
}

auto PsnCloudDesk::snapshotOf(const cloud::FetchFailure& failure) const -> cloud::Snapshot
{
    return services.cloud.snapshotOf(failure);
}

void PsnCloudDesk::refresh(bool force, Snapshotted done, Fetched fetched)
{
    services.cloud.refreshActiveProfile(force, std::move(done), std::move(fetched));
}

void PsnCloudDesk::clearCatalogCache()
{
    services.cloud.clearCatalogCache();
}

void PsnCloudDesk::details(const cloud::Game& game, Detailed done)
{
    services.cloud.detailsOf(game, lifetime.token(), std::move(done));
}

void PsnCloudDesk::checkStorefront(const cloud::Game& game, Checked done)
{
    services.cloud.checkStorefront(game, lifetime.token(), std::move(done));
}

void PsnCloudDesk::refreshTitle(const cloud::Game& game, Refreshed done)
{
    services.cloud.refreshTitle(game, lifetime.token(), std::move(done));
}

void PsnCloudDesk::clearForLaunch(const cloud::Game& game, Cleared cleared, Refused refused, Observed observed)
{
    static_cast<void>(launching.request_stop());
    launching = libchiaking::platform::StopSource{};
    services.cloud.clearForLaunch(game, launching.token(), std::move(cleared), std::move(refused),
        [observed = std::move(observed)](libchiaking::catalog::LaunchCheck check) {
            observed(cloud::LaunchChecks::checkOf(check));
        });
}

void PsnCloudDesk::cancelLaunch()
{
    static_cast<void>(launching.request_stop());
}

void PsnCloudDesk::launch(std::shared_ptr<cloud::CloudLaunch> cleared)
{
    if (!cleared || !play)
        return;
    auto host = std::make_shared<consoles::Host>(consoles::HostRecord{ .nickname = cleared->name() }, services.accounts,
        services.appSettings, services.dispatcher, services.core);
    host->setCloudLaunch(*cleared);
    play(std::move(host));
}

void PsnCloudDesk::signIn()
{
    if (openSignIn)
        openSignIn();
}

auto PsnCloudDesk::signedIn() const -> bool
{
    return services.cloud.signedIn();
}

auto PsnCloudDesk::nowMs() const -> std::int64_t
{
    const std::int64_t system = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch())
                                    .count();
    return services.cloud.trustedNowMs().value_or(system);
}

auto PsnCloudDesk::shortcuts() const -> std::vector<cloud::Game>
{
    return services.accounts.cloudShortcuts();
}

void PsnCloudDesk::setShortcuts(std::vector<cloud::Game> games)
{
    services.accounts.setCloudShortcuts(std::move(games));
    services.store.save();
}

auto PsnCloudDesk::filter() const -> std::string
{
    return services.cloudSettings.filter();
}

void PsnCloudDesk::setFilter(std::string key)
{
    services.cloudSettings.setFilter(std::move(key));
    services.store.save();
}

auto PsnCloudDesk::sortState() const -> int
{
    return services.cloudSettings.sortState();
}

void PsnCloudDesk::setSortState(int state)
{
    services.cloudSettings.setSortState(state);
    services.store.save();
}

}
