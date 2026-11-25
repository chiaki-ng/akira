#include "cloud/ui/cloud_model.hpp"

#include <algorithm>
#include <ctime>
#include <utility>

#include "cloud/cloud_shortcuts.hpp"
#include "cloud/release_notice.hpp"
#include "log/log.hpp"

namespace akira::cloud::ui {

CloudModel::CloudModel(CloudDesk& desk, app::Notifier& notifier, const app::Strings& strings)
    : desk(desk)
    , notifier(notifier)
    , strings(strings)
{
    if (auto saved = LibraryListing::filterNamed(desk.filter()))
        listing.pick(*saved);
    state.update([this](View& view) {
        view.sort = this->desk.sortState();
        view.shortcuts = this->desk.shortcuts();
    });
}

CloudModel::~CloudModel()
{
    *alive = false;
    desk.cancelLaunch();
}

auto CloudModel::statusKey(LibraryStatus::Chip chip) -> const char*
{
    switch (chip)
    {
        case LibraryStatus::Chip::Ready: return "akira/cloud/chip_ready";
        case LibraryStatus::Chip::Catalog: return "akira/cloud/chip_catalog";
        case LibraryStatus::Chip::Unavailable: return "akira/cloud/chip_unavailable";
        case LibraryStatus::Chip::Warning: return "akira/cloud/chip_warning";
        case LibraryStatus::Chip::Empty: return "akira/cloud/chip_empty";
        case LibraryStatus::Chip::Pair: return "akira/cloud/chip_pair";
        case LibraryStatus::Chip::Checking: return "akira/cloud/chip_checking";
        case LibraryStatus::Chip::Error: break;
    }
    return "akira/cloud/chip_error";
}

auto CloudModel::filterKey(LibraryFilter filter) -> const char*
{
    switch (filter)
    {
        case LibraryFilter::Recent: return "akira/cloud/filter_recent";
        case LibraryFilter::Owned: return "akira/cloud/filter_owned";
        case LibraryFilter::Plus: return "akira/cloud/filter_plus";
        case LibraryFilter::Browse: return "akira/cloud/filter_browse";
        case LibraryFilter::Preorders: return "akira/cloud/filter_preorders";
        case LibraryFilter::All: break;
    }
    return "akira/cloud/filter_all";
}

auto CloudModel::sortKey(LibraryOrder order) -> const char*
{
    switch (order)
    {
        case LibraryOrder::AToZ: return "akira/cloud/sort_az";
        case LibraryOrder::ZToA: return "akira/cloud/sort_za";
        case LibraryOrder::Playable: break;
    }
    return "akira/cloud/sort_playable";
}

auto CloudModel::stepKey(LaunchChecks::Step step) -> const char*
{
    switch (step)
    {
        case LaunchChecks::Step::Membership: return "akira/cloud/step_membership";
        case LaunchChecks::Step::Premium: return "akira/cloud/step_premium";
        case LaunchChecks::Step::YourCopy: return "akira/cloud/step_your_copy";
        case LaunchChecks::Step::CloudGame: return "akira/cloud/step_cloud_game";
        case LaunchChecks::Step::Restrictions: return "akira/cloud/step_restrictions";
        case LaunchChecks::Step::Release: break;
    }
    return "akira/cloud/step_release";
}

auto CloudModel::standingKey(GameActions::StandingNote note) -> const char*
{
    switch (note)
    {
        case GameActions::StandingNote::Demo: return "akira/cloud/standing_demo";
        case GameActions::StandingNote::Trial: return "akira/cloud/standing_trial";
        case GameActions::StandingNote::MayStream: return "akira/cloud/standing_may_stream";
        case GameActions::StandingNote::InLibrary: return "akira/cloud/standing_in_library";
        case GameActions::StandingNote::Included: return "akira/cloud/standing_included";
        case GameActions::StandingNote::Store: break;
    }
    return "akira/cloud/standing_store";
}

void CloudModel::appear()
{
    state.write(&View::shortcuts, desk.shortcuts());
    if (appeared && state.get().hasCatalog)
        return;
    appeared = true;
    refresh(false);
}

void CloudModel::refresh(bool force)
{
    const std::uint64_t operation = ++generation;
    fetch.follow(operation);
    render(desk.snapshot());
    desk.refresh(force,
        [this, guard = alive, operation](const Snapshot& snapshot) {
            if (!*guard || operation != generation)
                return;
            render(snapshot);
        },
        [this, guard = alive, operation](const FetchProgress& progress) {
            if (*guard)
                fetched(operation, progress);
        });
}

void CloudModel::fetched(std::uint64_t operation, const FetchProgress& progress)
{
    const FollowedFetch::Change change = fetch.observe(operation, progress);
    if (change == FollowedFetch::Change::Dropped || change == FollowedFetch::Change::Unchanged)
        return;
    if (progress.shown() == FetchProgress::Shown::Failure && progress.failure())
    {
        render(desk.snapshotOf(*progress.failure()));
        return;
    }
    state.write(&View::fetch, fetch.latest());
}

void CloudModel::render(const Snapshot& snapshot)
{
    const bool catalog = snapshot.hasCatalog && !snapshot.catalog.games.empty();
    state.update([&snapshot, catalog, this](View& view) {
        view.status = snapshot.status;
        view.library = LibraryStatus(snapshot.status);
        view.hasCatalog = catalog;
        view.fetch = fetch.latest();
    });
    if (!catalog)
    {
        listing.setGames({});
        state.update([](View& view) {
            view.games.clear();
            view.filters.clear();
            view.listed = 0;
        });
        return;
    }
    showCatalog(snapshot);
}

void CloudModel::showCatalog(const Snapshot& snapshot)
{
    listing.setNow(desk.nowMs());
    listing.setGames(snapshot.catalog.games);
    listing.setRecentlyPlayed(snapshot.recentlyPlayed);
    listing.settle(true);
    scheduleUnlock();
    applyFilter();
}

void CloudModel::scheduleUnlock()
{
    const auto wait = listing.unlockDelay();
    unlockAt.reset();
    if (wait)
        unlockAt = Clock::now() + std::chrono::milliseconds(*wait);
}

void CloudModel::applyFilter()
{
    std::vector<Game> ordered = listing.ordered(LibraryListing::orderOf(state.get().sort));
    state.update([this, &ordered](View& view) {
        view.games = std::move(ordered);
        view.filters = listing.filters();
        view.filter = listing.filter();
        view.platform = listing.platform();
        view.query = listing.query();
        view.listed = listing.listed().size();
    });
}

void CloudModel::tick(Clock::time_point now)
{
    if (unlockAt && now >= *unlockAt)
    {
        unlockAt.reset();
        log::info("CloudLib: a scheduled release is due, reloading the library");
        refresh(true);
    }
    if (!state.get().detail)
        return;
    updateRefreshing(now);
    updateRelease();
}

void CloudModel::pickFilter(LibraryFilter filter)
{
    listing.pick(filter);
    desk.setFilter(std::string(LibraryListing::key(filter)));
    applyFilter();
}

void CloudModel::pickPlatform(PlatformFilter platform)
{
    listing.setPlatform(platform);
    applyFilter();
}

void CloudModel::search(std::string text)
{
    listing.setQuery(std::move(text));
    applyFilter();
}

void CloudModel::sortBy(int sort)
{
    desk.setSortState(sort);
    state.write(&View::sort, sort);
    applyFilter();
}

void CloudModel::clearCache()
{
    desk.clearCatalogCache();
    notifier.notify(strings.text("akira/cloud/cache_cleared"));
    refresh(true);
}

void CloudModel::signIn()
{
    desk.signIn();
}

auto CloudModel::pinned(const Game& game) const -> bool
{
    return std::ranges::any_of(state.get().shortcuts,
        [&game](const Game& shortcut) { return shortcut.productId == game.productId; });
}

auto CloudModel::lastPlayed(const Game& game) const -> std::optional<std::int64_t>
{
    return listing.lastPlayed(game);
}

void CloudModel::toggleShortcut(const Game& game)
{
    CloudShortcuts shortcuts(desk.shortcuts());
    bool added = false;
    switch (shortcuts.toggle(game))
    {
        case CloudShortcuts::Toggle::Ignored: return;
        case CloudShortcuts::Toggle::NotBuilt:
            notifier.notify(strings.text("akira/cloud/launch_not_built"));
            return;
        case CloudShortcuts::Toggle::NeedsLibrary:
            state.write(&View::code,
                std::optional<Code>(Code{ .title = game.name,
                    .message = strings.format("akira/cloud/add_game_body", game.name),
                    .url = game.conceptUrl }));
            return;
        case CloudShortcuts::Toggle::Removed: added = false; break;
        case CloudShortcuts::Toggle::Added: added = true; break;
    }
    desk.setShortcuts(shortcuts.games());
    state.write(&View::shortcuts, shortcuts.games());
    notifier.notify(strings.text(added ? "akira/cloud/shortcut_added" : "akira/cloud/shortcut_removed"));
}

void CloudModel::open(const Game& game)
{
    desk.cancelLaunch();
    state.write(&View::detail, std::optional<Detail>(Detail{ .game = game }));
    updateRelease();
    loadDetails();
    startCardCheck();
}

void CloudModel::close()
{
    desk.cancelLaunch();
    state.update([](View& view) {
        view.detail.reset();
        view.code.reset();
    });
}

void CloudModel::loadDetails()
{
    const std::string product = state.get().detail->game.productId;
    desk.details(state.get().detail->game, [this, guard = alive, product](std::optional<GameDetail> facts) {
        if (!*guard || !facts || !state.get().detail || state.get().detail->game.productId != product)
            return;
        state.update([&facts](View& view) { view.detail->facts = std::move(facts); });
    });
}

void CloudModel::startCardCheck()
{
    const Detail& shown = *state.get().detail;
    if (!CardCheck::needed(shown.game) || (shown.card && shown.card->isSettled()) || !desk.signedIn())
        return;
    const std::string product = shown.game.productId;
    state.update([](View& view) { view.detail->card = CardCheck::checking(); });
    desk.checkStorefront(shown.game, [this, guard = alive, product](CardCheck answer) {
        if (!*guard || !state.get().detail || state.get().detail->game.productId != product)
            return;
        state.update([&answer](View& view) { view.detail->card = std::move(answer); });
    });
}

auto CloudModel::actions() const -> std::optional<GameActions>
{
    const auto& detail = state.get().detail;
    if (!detail)
        return std::nullopt;
    return GameActions(detail->game, desk.signedIn(), detail->card, cooldown.idle());
}

void CloudModel::play()
{
    const auto& detail = state.get().detail;
    const std::optional<GameActions> shown = actions();
    if (!detail || !shown || !shown->play() || !shown->play()->enabled())
        return;
    if (shown->play()->offer == GameActions::Offer::ChooseGame)
    {
        std::vector<Game> choices = GameActions::bundleChoices(detail->game, listing.listed());
        if (choices.empty())
            return;
        state.update([&choices](View& view) {
            view.detail->bundle = std::move(choices);
            view.detail->choosing = true;
        });
        return;
    }
    checkAndPlay(detail->game);
}

void CloudModel::choose(const Game& game)
{
    dismissChoice();
    checkAndPlay(game);
}

void CloudModel::retry(const Game& previous)
{
    const std::vector<Game>& listed = listing.listed();
    const auto known = std::ranges::find(listed, previous.productId, &Game::productId);
    const Game game = known == listed.end() ? previous : *known;
    const auto& detail = state.get().detail;
    const bool showing = detail
        && (detail->game.productId == game.productId || (detail->checks && detail->checks->productId() == game.productId));
    if (!showing)
        open(game);
    checkAndPlay(game);
}

void CloudModel::dismissChoice()
{
    if (state.get().detail)
        state.update([](View& view) { view.detail->choosing = false; });
}

void CloudModel::checkAndPlay(const Game& target)
{
    const auto& detail = state.get().detail;
    if (!detail || (detail->checks && detail->checks->isRunning()))
        return;
    const std::uint64_t attempt = ++attempts;
    state.update([&target, attempt](View& view) {
        view.detail->checks.emplace(target.productId, attempt, view.detail->game);
    });
    const auto current = [this, guard = alive, attempt] {
        return *guard && state.get().detail && state.get().detail->checks && state.get().detail->checks->attempt() == attempt;
    };
    desk.clearForLaunch(target,
        [this, current](std::shared_ptr<CloudLaunch> cleared) {
            if (!current())
                return;
            state.update([](View& view) { view.detail->checks->clear(); });
            desk.launch(std::move(cleared));
            state.update([](View& view) { view.detail->checks->launched(); });
        },
        [this, current](std::string reason) {
            if (!current())
                return;
            state.update([&reason](View& view) { view.detail->checks->refuse(std::move(reason)); });
        },
        [this, current](LaunchChecks::Check check) {
            if (!current())
                return;
            state.update([check](View& view) { view.detail->checks->observe(check); });
        });
}

void CloudModel::showStore(bool addingToLibrary)
{
    const auto& detail = state.get().detail;
    if (!detail || detail->game.conceptUrl.empty())
        return;
    const Game& game = detail->game;
    state.write(&View::code,
        std::optional<Code>(Code{ .title = game.name,
            .message = addingToLibrary ? strings.format("akira/cloud/detail_add_library_body", game.name, game.conceptUrl)
                                       : strings.format("akira/cloud/detail_store_body", game.conceptUrl),
            .url = game.conceptUrl }));
}

void CloudModel::dismissCode()
{
    state.write(&View::code, std::optional<Code>());
}

void CloudModel::refreshTitle()
{
    const auto& detail = state.get().detail;
    if (!detail || !detail->game.canBeAddedToLibrary)
        return;
    const auto started = cooldown.begin(Clock::now());
    if (!started)
        return;
    updateRefreshing(Clock::now());
    const std::string product = detail->game.productId;
    desk.refreshTitle(detail->game, [this, guard = alive, attempt = *started, product](TitleRefresh refreshed) {
        if (!*guard || !cooldown.finish(attempt, refreshed, Clock::now()))
            return;
        if (state.get().detail && state.get().detail->game.productId == product && refreshed.outcome == TitleRefresh::Outcome::Added
            && refreshed.game)
            state.update([&refreshed](View& view) { view.detail->game = *refreshed.game; });
        updateRefreshing(Clock::now());
    });
}

void CloudModel::updateRefreshing(Clock::time_point now)
{
    static_cast<void>(cooldown.tick(now));
    Refreshing next{ .phase = cooldown.phase(),
        .seconds = cooldown.secondsLeft(now),
        .outcome = cooldown.outcome(),
        .message = cooldown.message() };
    if (state.get().detail && state.get().detail->refreshing != next)
        state.update([&next](View& view) { view.detail->refreshing = std::move(next); });
}

void CloudModel::updateRelease()
{
    const auto& detail = state.get().detail;
    if (!detail)
        return;
    std::optional<std::string> text = releaseText(detail->game);
    if (text != detail->release)
        state.update([&text](View& view) { view.detail->release = std::move(text); });
}

auto CloudModel::releaseText(const Game& game) const -> std::optional<std::string>
{
    const auto notice = ReleaseNotice::of(game, desk.nowMs());
    if (!notice)
        return std::nullopt;
    switch (notice->kind())
    {
        case ReleaseNotice::Kind::Unannounced: return strings.text("akira/cloud/release_unannounced");
        case ReleaseNotice::Kind::Countdown: return strings.format("akira/cloud/release_in", Game::countdown(notice->seconds()));
        case ReleaseNotice::Kind::Date: break;
    }
    const std::time_t at = static_cast<std::time_t>(notice->atMs() / 1000);
    std::tm local{};
    localtime_r(&at, &local);
    char text[64] = {};
    std::strftime(text, sizeof(text), "%d %b %Y %H:%M", &local);
    return strings.format("akira/cloud/release_on", std::string(text));
}

}
