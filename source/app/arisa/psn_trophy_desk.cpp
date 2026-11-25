#include "app/arisa/psn_trophy_desk.hpp"

#include <expected>
#include <string>
#include <utility>

#include "accounts/account_settings.hpp"
#include "accounts/accounts.hpp"
#include "log/log.hpp"
#include "play/chiaki_core.hpp"
#include "trophies/trophy_manager.hpp"

namespace akira::app::arisa {

PsnTrophyDesk::PsnTrophyDesk(Services& services)
    : services(services)
{
}

auto PsnTrophyDesk::linked() const -> bool
{
    accounts::Accounts* signedIn = services.core.accounts();
    return signedIn && signedIn->state() != accounts::SessionState::NotLinked;
}

auto PsnTrophyDesk::enabled() const -> bool
{
    return services.accounts.activeProfileTrophiesEnabled();
}

template <typename Value>
auto PsnTrophyDesk::failed(Answer<Value> done) -> std::function<void(accounts::Status, const std::string&)>
{
    return [done = std::move(done)](accounts::Status status, const std::string& message) {
        log::warning("[TROPHY] request failed [{}] {}", accounts::statusName(status), message);
        done(std::unexpected(status));
    };
}

void PsnTrophyDesk::summary(bool force, Answer<Summary> done)
{
    services.trophies.fetchSummary(force, [done](const Summary& shown) { done(shown); }, failed(done));
}

void PsnTrophyDesk::library(bool force, Answer<std::vector<Title>> done)
{
    services.trophies.fetchLibrary(force, [done](const std::vector<Title>& titles) { done(titles); }, failed(done));
}

void PsnTrophyDesk::detail(const Title& title, bool force, Answer<Detail> done)
{
    services.trophies.fetchTitleDetail(title, force, [done](const Detail& shown) { done(shown); }, failed(done));
}

auto PsnTrophyDesk::played(const TitleId& title) const -> std::optional<Played>
{
    const std::optional<trophies::TrophyManager::PlayedGame> game = services.trophies.playedGameFor(title);
    if (!game)
        return std::nullopt;
    Played made{ .time = game->play_time };
    if (game->last_played)
        made.last = std::chrono::time_point_cast<std::chrono::system_clock::duration>(*game->last_played);
    return made;
}

void PsnTrophyDesk::resolvePlayed(const std::vector<Title>& titles, std::function<void()> updated)
{
    services.trophies.resolvePlayedGames(titles, std::move(updated));
}

auto PsnTrophyDesk::cooldown(std::string_view scope) -> std::optional<std::chrono::seconds>
{
    const accounts::ActionStatus status = services.trophies.forceRefreshStatus(std::string(scope));
    if (status.state != accounts::ActionState::CoolingDown)
        return std::nullopt;
    return std::chrono::seconds{ status.secondsRemaining };
}

void PsnTrophyDesk::recordRefresh(std::string_view scope)
{
    services.trophies.recordForcedRefresh(std::string(scope));
}

auto PsnTrophyDesk::savedAt() const -> std::int64_t
{
    return services.trophies.librarySavedAtSeconds();
}

}
