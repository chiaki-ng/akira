#include "trophies/ui/trophy_model.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <utility>

#include "log/log.hpp"

namespace akira::trophies::ui {

TrophyModel::TrophyModel(TrophyDesk& desk, app::Notifier& notifier, const app::Strings& strings)
    : desk(desk)
    , notifier(notifier)
    , strings(strings)
{
}

TrophyModel::~TrophyModel()
{
    *alive = false;
}

auto TrophyModel::nowSeconds() -> std::int64_t
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

auto TrophyModel::errorKey(accounts::Status status) -> const char*
{
    switch (status)
    {
        case accounts::Status::NotLinked: return "akira/trophies/error_not_linked";
        case accounts::Status::SessionExpired: return "akira/trophies/error_session_expired";
        case accounts::Status::Offline: return "akira/trophies/error_offline";
        case accounts::Status::RateLimited: return "akira/trophies/error_rate_limited";
        case accounts::Status::Ok:
        case accounts::Status::ServerError: break;
    }
    return "akira/trophies/error_generic";
}

auto TrophyModel::waitShown(int seconds) -> std::string
{
    if (seconds < 60)
        return std::format("{}s", seconds);
    if (seconds < 3600)
        return std::format("{}m", (seconds + 59) / 60);
    int hours = seconds / 3600;
    int minutes = ((seconds % 3600) + 59) / 60;
    if (minutes == 60)
    {
        ++hours;
        minutes = 0;
    }
    return minutes > 0 ? std::format("{}h {}m", hours, minutes) : std::format("{}h", hours);
}

auto TrophyModel::waitText(int seconds) const -> std::string
{
    return strings.format("akira/trophies/force_refresh_wait", waitShown(seconds));
}

void TrophyModel::appear()
{
    if (!desk.linked())
    {
        state.write(&View::phase, Phase::NotLinked);
        requested = false;
        return;
    }
    if (!desk.enabled())
    {
        state.write(&View::phase, Phase::Disabled);
        requested = false;
        return;
    }
    if (state.get().phase == Phase::NotLinked || state.get().phase == Phase::Disabled)
        state.write(&View::phase, titles.empty() ? Phase::Loading : Phase::Ready);
    View next = state.get();
    refreshGates(next);
    state.set(std::move(next));
    if (!requested)
        load(false);
}

void TrophyModel::reset()
{
    ++pageGeneration;
    titles.clear();
    detail.reset();
    requested = false;
    stamped.reset();
    state.set(View{});
}

void TrophyModel::tick(Clock::time_point now)
{
    View next = state.get();
    refreshGates(next);
    if (!stamped || now - *stamped >= updatedEvery)
    {
        stamped = now;
        next.updated = updatedText(desk.savedAt(), nowSeconds());
    }
    state.set(std::move(next));
}

void TrophyModel::refreshGates(View& next)
{
    next.wait.reset();
    if (const auto left = desk.cooldown(TrophyDesk::libraryScope))
        next.wait = static_cast<int>(left->count());
    if (next.page)
    {
        next.page->wait.reset();
        if (const auto left = desk.cooldown(next.page->title.id.text()))
            next.page->wait = static_cast<int>(left->count());
    }
}

void TrophyModel::load(bool force)
{
    if (state.get().refreshing)
    {
        log::info("Trophy grid: load already in progress, ignoring request");
        return;
    }
    requested = true;
    state.update([this](View& view) {
        view.refreshing = true;
        if (titles.empty())
            view.phase = Phase::Loading;
    });
    desk.summary(force, [this, guard = alive](std::expected<Summary, accounts::Status> answer) {
        if (!*guard)
            return;
        if (!answer)
        {
            log::warning("Trophy grid: summary unavailable [{}]", accounts::statusName(answer.error()));
            state.write(&View::summaryFailed, true);
            return;
        }
        state.update([&answer](View& view) {
            view.summary = std::move(*answer);
            view.summaryFailed = false;
        });
    });
    desk.library(force, [this, guard = alive](std::expected<std::vector<Title>, accounts::Status> answer) {
        if (!*guard)
            return;
        state.write(&View::refreshing, false);
        if (!answer)
        {
            const char* key = errorKey(answer.error());
            log::error("Trophy grid: load failed [{}]", accounts::statusName(answer.error()));
            if (titles.empty())
            {
                state.update([this, key](View& view) {
                    view.phase = Phase::Failed;
                    view.error = strings.text(key);
                });
                return;
            }
            notifier.notify(strings.text(key));
            return;
        }
        titles = std::move(*answer);
        rebuild();
        desk.resolvePlayed(titles, [this, guard] {
            if (*guard)
                rebuild();
        });
    });
}

void TrophyModel::rebuild()
{
    View next = state.get();
    if (next.phase != Phase::NotLinked && next.phase != Phase::Disabled)
        next.phase = Phase::Ready;
    next.anyTitles = !titles.empty();
    next.rows.clear();
    for (Title& title : next.listing.listed(titles))
    {
        std::string played = playedText(title);
        next.rows.push_back(Row{ .title = std::move(title), .played = std::move(played) });
    }
    next.updated = updatedText(desk.savedAt(), nowSeconds());
    refreshGates(next);
    state.set(std::move(next));
}

void TrophyModel::refresh()
{
    if (state.get().refreshing || desk.cooldown(TrophyDesk::libraryScope))
        return;
    log::info("User triggered force refresh");
    notifier.notify(strings.text("akira/trophies/force_refresh"));
    desk.recordRefresh(TrophyDesk::libraryScope);
    load(true);
}

void TrophyModel::retry()
{
    load(false);
}

void TrophyModel::setSort(TitleListing::Sort sort)
{
    state.write(&View::listing, TitleListing(sort, state.get().listing.filter()));
    rebuild();
}

void TrophyModel::setFilter(TitleListing::Filter filter)
{
    state.write(&View::listing, TitleListing(state.get().listing.sort(), filter));
    rebuild();
}

void TrophyModel::open(const std::string& titleId)
{
    const auto found = std::ranges::find_if(titles, [&titleId](const Title& title) { return title.id == titleId; });
    if (found == titles.end())
        return;
    openPage(*found, false);
}

void TrophyModel::openPage(const Title& title, bool force)
{
    const std::uint64_t generation = ++pageGeneration;
    const bool same = detail && detail->title == title.id;
    if (!same)
        detail.reset();
    state.update([&title, same](View& view) {
        Page page = same && view.page ? *view.page : Page{ .title = title };
        page.title = title;
        page.loading = true;
        page.refreshing = true;
        page.error.clear();
        if (!same)
            view.trophyListing = TrophyListing(view.trophyListing.sort(), view.trophyListing.filter(), std::nullopt);
        view.page = std::move(page);
    });
    View next = state.get();
    rebuildPage(next);
    refreshGates(next);
    state.set(std::move(next));
    desk.detail(title, force, [this, guard = alive, generation](std::expected<Detail, accounts::Status> answer) {
        if (!*guard || generation != pageGeneration || !state.get().page)
            return;
        if (!answer)
        {
            const char* key = errorKey(answer.error());
            log::error("Trophy detail: load failed [{}]", accounts::statusName(answer.error()));
            state.update([this, key](View& view) {
                view.page->loading = false;
                view.page->refreshing = false;
                view.page->error = strings.text(key);
            });
            return;
        }
        log::info("Trophy detail {}: {} trophies across {} group(s)", answer->title.text(), answer->trophies.size(),
            answer->groups.size());
        detail = std::move(*answer);
        View loaded = state.get();
        loaded.page->loading = false;
        loaded.page->refreshing = false;
        rebuildPage(loaded);
        state.set(std::move(loaded));
    });
}

void TrophyModel::rebuildPage(View& next) const
{
    if (!next.page)
        return;
    Page& page = *next.page;
    page.earned = page.title.earned;
    page.defined = page.title.defined;
    page.progress = page.title.progress;
    if (!detail)
    {
        page.groups.clear();
        page.trophies.clear();
        return;
    }
    page.groups = TrophyListing::grouped(*detail) ? detail->groups : std::vector<Group>{};
    page.trophies = next.trophyListing.listed(*detail);
    if (next.trophyListing.group())
        if (const auto group = TrophyListing::groupOf(*detail, *next.trophyListing.group()))
        {
            page.earned = group->earned;
            page.defined = group->defined;
            page.progress = group->progress;
        }
}

void TrophyModel::close()
{
    ++pageGeneration;
    state.write(&View::page, std::optional<Page>());
}

void TrophyModel::selectGroup(std::optional<std::string> groupId)
{
    std::optional<TrophyListing::GroupId> group;
    if (groupId)
        group = TrophyListing::GroupId::parse(*groupId);
    View next = state.get();
    next.trophyListing = TrophyListing(next.trophyListing.sort(), next.trophyListing.filter(), std::move(group));
    rebuildPage(next);
    state.set(std::move(next));
}

void TrophyModel::setTrophySort(TrophyListing::Sort sort)
{
    View next = state.get();
    next.trophyListing = TrophyListing(sort, next.trophyListing.filter(), next.trophyListing.group());
    rebuildPage(next);
    state.set(std::move(next));
}

void TrophyModel::setTrophyFilter(TrophyListing::Filter filter)
{
    View next = state.get();
    next.trophyListing = TrophyListing(next.trophyListing.sort(), filter, next.trophyListing.group());
    rebuildPage(next);
    state.set(std::move(next));
}

void TrophyModel::reveal(std::uint32_t trophyId)
{
    state.update([trophyId](View& view) {
        if (!view.page)
            return;
        if (!view.page->revealed.insert(trophyId).second)
            view.page->revealed.erase(trophyId);
    });
}

void TrophyModel::refreshTitle()
{
    const std::optional<Page>& page = state.get().page;
    if (!page || page->refreshing)
        return;
    const Title title = page->title;
    const std::string scope = title.id.text();
    if (desk.cooldown(scope))
        return;
    log::info("User triggered force refresh of {}", scope);
    notifier.notify(strings.text("akira/trophies/force_refresh"));
    desk.recordRefresh(scope);
    openPage(title, true);
}

void TrophyModel::showTrophy(std::uint32_t trophyId)
{
    state.update([trophyId](View& view) {
        if (!view.page)
            return;
        const auto found = std::ranges::find_if(view.page->trophies,
            [trophyId](const Trophy& trophy) { return trophy.id.value() == trophyId; });
        if (found != view.page->trophies.end())
            view.page->shown = *found;
    });
}

void TrophyModel::closeTrophy()
{
    state.update([](View& view) {
        if (view.page)
            view.page->shown.reset();
    });
}

auto TrophyModel::agoText(std::int64_t age) const -> std::string
{
    if (age < 90)
        return strings.text("akira/trophies/played_just_now");
    if (age < 3600)
        return strings.format("akira/trophies/played_minutes_ago", static_cast<int>(age / 60));
    if (age < 86400)
        return strings.format("akira/trophies/played_hours_ago", static_cast<int>(age / 3600));
    return strings.format("akira/trophies/played_days_ago", static_cast<int>(age / 86400));
}

auto TrophyModel::playedText(const Title& title) const -> std::string
{
    const std::optional<TrophyDesk::Played> played = desk.played(title.id);
    if (!played || !played->time || played->time->count() <= 0)
        return {};
    const std::int64_t seconds = played->time->count();
    const std::int64_t hours = seconds / 3600;
    std::string duration = hours >= 1 ? strings.format("akira/trophies/play_hours", static_cast<int>(hours))
                                      : strings.format("akira/trophies/play_minutes", static_cast<int>((seconds + 59) / 60));
    if (!played->last)
        return duration;
    const std::int64_t last =
        std::chrono::duration_cast<std::chrono::seconds>(played->last->time_since_epoch()).count();
    return strings.format("akira/trophies/play_and_last", duration, agoText(nowSeconds() - last));
}

auto TrophyModel::updatedText(std::int64_t savedAt, std::int64_t now) const -> std::string
{
    if (savedAt <= 0)
        return {};
    const std::int64_t age = now - savedAt;
    if (age < 90)
        return strings.text("akira/trophies/updated_just_now");
    if (age < 3600)
        return strings.format("akira/trophies/updated_minutes", static_cast<int>(age / 60));
    if (age < 86400)
        return strings.format("akira/trophies/updated_hours", static_cast<int>(age / 3600));
    return strings.format("akira/trophies/updated_days", static_cast<int>(age / 86400));
}

auto TrophyModel::rarityText(const Trophy& trophy) const -> std::string
{
    const std::string name =
        strings.text(TrophyListing::rarityKey(trophy.rarity.value_or(libchiaking::trophies::Rarity::common)));
    const double rate = trophy.earned_rate.value_or(0.0);
    if (rate <= 0.0)
        return name;
    return std::format("{}  ·  {:.1f}%", name, rate);
}

auto TrophyModel::earnedText(const Trophy& trophy) const -> std::string
{
    if (!trophy.earned)
        return {};
    if (!trophy.earned_at)
        return strings.text("akira/trophies/earned");
    return std::format("{}  {:%F}", strings.text("akira/trophies/earned"),
        std::chrono::floor<std::chrono::days>(*trophy.earned_at));
}

}
