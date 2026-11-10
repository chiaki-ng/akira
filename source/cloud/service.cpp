#include "cloud/service.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>

#include <chrono>
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <variant>
#include <limits>
#include <system_error>
#include <utility>

#include "core/chiaki_core.hpp"
#include "core/settings_manager.hpp"
#include "util/http_pool.hpp"

using namespace brls::literals;

namespace cloud {

namespace catalog = libchiaking::catalog;
namespace platform = libchiaking::platform;

Service& Service::instance()
{
    static Service instance;
    return instance;
}

Service::Service()
{
    settings = SettingsManager::getInstance();
    std::error_code ignored;
    if (std::filesystem::remove_all(LEGACY_CACHE_DIR, ignored) > 0)
        brls::Logger::info("Cloud: removed the old catalog cache");
    backgroundSubscription = akira::BackgroundWork::subscribe([this](bool paused) { followBackgroundWork(paused); });
}

void Service::followBackgroundWork(bool nowPaused)
{
    if (nowPaused)
    {
        bool stopped = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            paused = true;
            for (auto& [profileId, entry] : entries)
            {
                if (!entry.refresh)
                    continue;
                static_cast<void>(entry.refresh->stop.request_stop());
                std::ranges::move(entry.refresh->waiters, std::back_inserter(held.waiters));
                entry.refresh.reset();
                held.wanted = true;
                stopped = true;
            }
        }
        if (stopped)
            brls::Logger::info("Cloud: catalog refresh stopped for the stream");
        return;
    }

    Held resumed;
    {
        std::lock_guard<std::mutex> lock(mutex);
        paused = false;
        resumed = std::exchange(held, Held{});
    }
    if (!resumed.wanted)
        return;
    brls::Logger::info("Cloud: catalog refresh resumed after the stream");
    if (resumed.waiters.empty())
    {
        refreshActiveProfile(resumed.force);
        return;
    }
    bool first = true;
    for (Waiter& waiter : resumed.waiters)
    {
        refreshActiveProfile(first && resumed.force, std::move(waiter.onDone), std::move(waiter.onFetch));
        first = false;
    }
}

auto Service::catalogOf() -> catalog::Catalog&
{
    return akira::ChiakiCore::current().chiaki().catalog();
}

auto Service::activeUser() const -> User
{
    psn::Accounts* accounts = akira::ChiakiCore::current().accounts();
    User user = accounts ? accounts->activeUser() : nullptr;
    return user && user->has_psn_account() ? user : nullptr;
}

std::optional<int64_t> Service::trustedNowMs() const
{
    if (!akira::ChiakiCore::current().opened())
        return std::nullopt;
    const auto now = catalogOf().trusted_time();
    if (!now)
        return std::nullopt;
    return now->time_since_epoch().count();
}

Snapshot Service::defaultSnapshotForProfile(bool hasProfile, bool paired)
{
    Snapshot snapshot;
    if (!hasProfile)
    {
        snapshot.status.availability = Availability::NoProfile;
        snapshot.status.title = "akira/cloud/status_no_profile_title"_i18n;
        snapshot.status.detail = "akira/cloud/status_no_profile_detail"_i18n;
        return snapshot;
    }

    if (!paired)
    {
        snapshot.status.availability = Availability::NeedsPairing;
        snapshot.status.title = "akira/cloud/status_pair_title"_i18n;
        snapshot.status.detail = "akira/cloud/status_pair_detail"_i18n;
        snapshot.status.canPair = true;
        return snapshot;
    }

    snapshot.status.availability = Availability::Checking;
    snapshot.status.title = "akira/cloud/status_checking_title"_i18n;
    snapshot.status.detail = "akira/cloud/status_checking_detail"_i18n;
    return snapshot;
}

Status Service::statusOf(const Catalog& catalog, PlusMembership plusMembership)
{
    Status status;
    status.gameCount = catalog.launchableCount();
    status.plusMembership = plusMembership;

    if (status.gameCount == 0)
    {
        status.availability = Availability::Empty;
        status.title = "akira/cloud/status_empty_title"_i18n;
        status.detail = "akira/cloud/status_empty_detail"_i18n;
        status.canBrowse = true;
        return status;
    }

    if (plusMembership == PlusMembership::None)
    {
        status.availability = Availability::SubscriptionRequired;
        status.title = "akira/cloud/status_subscription_title"_i18n;
        status.detail = "akira/cloud/status_subscription_detail"_i18n;
        status.canBrowse = true;
        status.degraded = true;
        return status;
    }

    status.availability = Availability::CatalogAvailable;
    status.title = brls::getStr("akira/cloud/status_catalog_title", status.gameCount);
    status.detail = "akira/cloud/status_catalog_detail"_i18n;
    status.canBrowse = true;
    return status;
}

Game Service::gameOf(const catalog::MyGame& game, catalog::Catalog& catalog, libchiaking::users::PsUser& user)
{
    const catalog::Title& title = game.title();

    Game out;
    out.productId = game.product_id().text();
    out.name = title.name();
    out.imageUrl = title.cover_url();
    out.landscapeImageUrl = title.landscape_cover_url();
    if (title.concept_id())
        out.conceptId = title.concept_id()->text();
    if (title.concept_page())
        out.conceptUrl = title.concept_page()->url();

    switch (title.platform())
    {
        case catalog::Platform::ps5: out.platform = "ps5"; break;
        case catalog::Platform::ps4: out.platform = "ps4"; break;
        case catalog::Platform::ps3: out.platform = "ps3"; break;
    }

    out.serviceType = title.service() == catalog::ServiceGeneration::ps5_cloud ? "pscloud" : "psnow";
    out.streamServiceType = out.serviceType;
    out.launchLeftOut = launchLeftOut(out.serviceType);
    out.streamIdentifier = game.launch_identifier().text();
    out.isOwned = game.is_owned();
    if (game.ownership())
        out.entitlementId = game.ownership()->id().text();
    out.plusCatalog = game.inclusion().is_included();
    out.needsLibraryAdd = game.needs_library_add();

    if (out.isOwned)
        out.category = "owned";
    else if (game.standing() == catalog::Standing::store)
        out.category = "purchaseable";
    else
        out.category = "streamable";

    const catalog::ReleaseDate release = game.release();
    if (release.is_unannounced())
    {
        out.release = ReleaseKind::Unannounced;
        out.availableFromMs = std::numeric_limits<int64_t>::max();
    }
    else if (const auto at = release.time(); at && !release.is_unrestricted())
    {
        out.release = ReleaseKind::Scheduled;
        out.availableFromMs = at->time_since_epoch().count();
    }

    out.standing = standingOf(game.standing());
    out.canBeAddedToLibrary = game.can_be_added_to_library();
    out.needsStorefrontCheck = game.needs_storefront_check();
    out.refused = game.remembered_refusal().has_value();
    if (game.ownership())
    {
        out.isTrial = title.service() == catalog::ServiceGeneration::ps5_cloud &&
                      game.ownership()->held_as() == catalog::HeldAs::trial_access;
        out.trialEnded = out.isTrial && game.ownership()->trial_ended();
    }
    for (const auto& child : game.children())
    {
        BundledGame bundled;
        bundled.entitlementId = child.entitlement.text();
        if (child.product)
        {
            bundled.productId = child.product->text();
            bundled.launchable = catalog.can_launch(user, *child.product).is_allowed();
        }
        out.bundle.push_back(std::move(bundled));
    }

    const catalog::LaunchVerdict verdict = catalog.can_launch(user, game.product_id());
    if (const catalog::LaunchRefusal* refusal = verdict.refusal())
        out.launchBlock = refusal->kind() == catalog::LaunchRefusal::Kind::choose_a_game ? LaunchBlock::ChooseAGame
                                                                                         : LaunchBlock::Refused;
    if (game.remembered_refusal() || title.streamability() == catalog::ListStreamability::marked_unstreamable)
        out.streamabilityStatus = StreamabilityStatus::NotStreamable;
    else if (verdict.is_allowed() && title.streamability() == catalog::ListStreamability::listed)
        out.streamabilityStatus = StreamabilityStatus::Streamable;

    return out;
}

Standing Service::standingOf(catalog::Standing standing)
{
    switch (standing)
    {
        case catalog::Standing::may_stream: return Standing::MayStream;
        case catalog::Standing::in_library: return Standing::InLibrary;
        case catalog::Standing::included: return Standing::Included;
        case catalog::Standing::store: return Standing::Store;
        case catalog::Standing::ownership_unknown: return Standing::OwnershipUnknown;
    }
    return Standing::MayStream;
}

auto Service::recentlyPlayedOf(libchiaking::users::PsUser& user) -> std::vector<RecentlyPlayed>
{
    auto played = catalogOf().recently_played(user);
    if (!played)
    {
        brls::Logger::warning("Cloud: recently played unanswered (reason {})", static_cast<int>(played.error().reason()));
        return {};
    }
    std::vector<RecentlyPlayed> out;
    out.reserve(played->size());
    for (const catalog::RecentlyPlayedGame& game : *played)
        out.push_back({game.product.text(),
                       std::chrono::duration_cast<std::chrono::milliseconds>(game.last_played.time_since_epoch()).count()});
    return out;
}

Snapshot Service::snapshotOf(const catalog::MyGames& games, catalog::Catalog& catalog,
                             libchiaking::users::PsUser& user, PlusMembership plusMembership)
{
    Snapshot snapshot;
    snapshot.catalog.games.reserve(games.visible().size());
    for (const catalog::MyGame& game : games.visible())
        snapshot.catalog.games.push_back(gameOf(game, catalog, user));
    snapshot.status = statusOf(snapshot.catalog, plusMembership);
    snapshot.hasCatalog = !snapshot.catalog.games.empty();
    return snapshot;
}

auto Service::failureOf(const catalog::GamesFailure& cause) -> FetchFailure
{
    using Reason = catalog::GamesReason;
    FetchFailure::Reason reason = FetchFailure::Reason::Unavailable;
    switch (cause.reason())
    {
        case Reason::cancelled: reason = FetchFailure::Reason::Cancelled; break;
        case Reason::user: reason = FetchFailure::Reason::User; break;
        case Reason::unavailable: reason = FetchFailure::Reason::Unavailable; break;
        case Reason::rejected: reason = FetchFailure::Reason::Rejected; break;
        case Reason::region_unsupported: reason = FetchFailure::Reason::RegionUnsupported; break;
        case Reason::malformed: reason = FetchFailure::Reason::Malformed; break;
        case Reason::not_built: reason = FetchFailure::Reason::Unavailable; break;
    }
    return FetchFailure(reason, cause.http_status(), cause.needs_sign_in());
}

auto Service::stepOf(catalog::FetchStep step) -> FetchProgress::Step
{
    using Step = FetchProgress::Step;
    switch (step)
    {
        case catalog::FetchStep::idle: return Step::Idle;
        case catalog::FetchStep::checking_sign_in: return Step::CheckingSignIn;
        case catalog::FetchStep::cloud_list: return Step::CloudList;
        case catalog::FetchStep::owned_library: return Step::OwnedLibrary;
        case catalog::FetchStep::premium_category: return Step::PremiumCategory;
        case catalog::FetchStep::for_sale_category: return Step::ForSaleCategory;
        case catalog::FetchStep::streaming_access: return Step::StreamingAccess;
        case catalog::FetchStep::matching: return Step::Matching;
        case catalog::FetchStep::trusted_time: return Step::TrustedTime;
        case catalog::FetchStep::exclusions: return Step::Exclusions;
        case catalog::FetchStep::ready: return Step::Ready;
        case catalog::FetchStep::unanswered: return Step::Unanswered;
        case catalog::FetchStep::cancelled: return Step::Cancelled;
    }
    return Step::Idle;
}

auto Service::activityOf(catalog::FetchActivity activity) -> FetchProgress::Activity
{
    using Activity = FetchProgress::Activity;
    switch (activity)
    {
        case catalog::FetchActivity::reusing_held_answer: return Activity::ReusingHeldAnswer;
        case catalog::FetchActivity::walking_public_store: return Activity::WalkingPublicStore;
        case catalog::FetchActivity::walking_old_service_shelves: return Activity::WalkingOldServiceShelves;
        case catalog::FetchActivity::walking_public_store_fallback: return Activity::WalkingPublicStoreFallback;
        case catalog::FetchActivity::checking_streaming_copies: return Activity::CheckingStreamingCopies;
        case catalog::FetchActivity::reading_ps5_store_lists: return Activity::ReadingPs5StoreLists;
        case catalog::FetchActivity::reading_entitlements: return Activity::ReadingEntitlements;
        case catalog::FetchActivity::reading_streamed_ps4_concepts: return Activity::ReadingStreamedPs4Concepts;
        case catalog::FetchActivity::scanning_ps5_library: return Activity::ScanningPs5Library;
        case catalog::FetchActivity::reading_category_pages: return Activity::ReadingCategoryPages;
        case catalog::FetchActivity::reading_subscriptions: return Activity::ReadingSubscriptions;
        case catalog::FetchActivity::matching_games: return Activity::MatchingGames;
        case catalog::FetchActivity::syncing_clock: return Activity::SyncingClock;
        case catalog::FetchActivity::reading_exclusions: return Activity::ReadingExclusions;
    }
    return Activity::ReusingHeldAnswer;
}

auto Service::outcomeOf(catalog::StepOutcome outcome) -> FetchProgress::Outcome
{
    switch (outcome)
    {
        case catalog::StepOutcome::skipped: return FetchProgress::Outcome::Skipped;
        case catalog::StepOutcome::held: return FetchProgress::Outcome::Held;
        case catalog::StepOutcome::fetched: return FetchProgress::Outcome::Fetched;
        case catalog::StepOutcome::degraded: return FetchProgress::Outcome::Degraded;
    }
    return FetchProgress::Outcome::Skipped;
}

auto Service::progressOf(const catalog::FetchState& state) -> FetchProgress
{
    std::optional<FetchProgress::Activity> activity;
    if (state.activity())
        activity = activityOf(*state.activity());
    std::vector<FetchProgress::PassedStep> passed;
    passed.reserve(state.passed().size());
    for (const catalog::PassedStep& step : state.passed())
        passed.push_back({stepOf(step.step), outcomeOf(step.outcome)});
    std::optional<FetchFailure> failure;
    if (state.failure())
        failure = failureOf(*state.failure());
    return FetchProgress(state.attempt(), stepOf(state.step()), activity, state.pages_read(), state.pages_expected(),
                         std::move(passed), std::move(failure), state.time_server());
}

Snapshot Service::snapshotOf(const FetchFailure& failure)
{
    Snapshot snapshot;
    Status& status = snapshot.status;

    if (failure.needsSignIn())
    {
        status.availability = Availability::Warning;
        status.title = "akira/cloud/status_relink_title"_i18n;
        status.detail = "akira/cloud/status_relink_detail"_i18n;
        status.canPair = true;
        status.degraded = true;
        return snapshot;
    }

    if (failure.reason() == FetchFailure::Reason::RegionUnsupported)
    {
        status.availability = Availability::Warning;
        status.title = "akira/cloud/status_region_title"_i18n;
        status.detail = "akira/cloud/status_region_detail"_i18n;
        status.degraded = true;
        return snapshot;
    }

    status.availability = Availability::Error;
    status.title = "akira/cloud/status_error_title"_i18n;
    status.detail = "akira/cloud/status_error_detail"_i18n;
    return snapshot;
}

Snapshot Service::snapshotForActiveProfile() const
{
    const Profile* profile = settings->getActiveProfile();
    if (!profile)
        return defaultSnapshotForProfile(false, false);

    std::lock_guard<std::mutex> lock(mutex);
    auto it = entries.find(profile->id);
    if (it == entries.end())
        return defaultSnapshotForProfile(true, profile->isRemote());

    return it->second.snapshot;
}

void Service::markActiveProfileDirty()
{
    const Profile* profile = settings->getActiveProfile();
    if (!profile)
        return;

    std::lock_guard<std::mutex> lock(mutex);
    auto it = entries.find(profile->id);
    if (it != entries.end() && !it->second.refresh)
        entries.erase(it);
}

void Service::clearCatalogCache()
{
    const Profile* profile = settings->getActiveProfile();
    if (!profile)
        return;

    if (User user = activeUser())
        catalogOf().start_over(*user);

    std::lock_guard<std::mutex> lock(mutex);
    auto it = entries.find(profile->id);
    if (it != entries.end() && !it->second.refresh)
        entries.erase(it);
}

void Service::refreshInBackground()
{
    if (!akira::ChiakiCore::current().opened())
        return;
    if (const Profile* profile = settings->getActiveProfile())
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto it = entries.find(profile->id);
        if (it != entries.end() && it->second.fetchedAt)
            return;
    }
    brls::Logger::info("Cloud: background catalog refresh after the PSN keep-alive");
    refreshActiveProfile(false);
}

void Service::refreshActiveProfile(bool force, SnapshotCallback onDone, FetchCallback onFetch)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (paused)
        {
            held.wanted = true;
            held.force = held.force || force;
            if (onDone || onFetch)
                held.waiters.push_back({std::move(onDone), std::move(onFetch)});
            return;
        }
    }

    const Profile* profile = settings->getActiveProfile();
    if (!profile)
    {
        Snapshot snapshot = defaultSnapshotForProfile(false, false);
        if (onDone)
            brls::sync([onDone, snapshot]() { onDone(snapshot); });
        return;
    }

    const int64_t profileId = profile->id;
    User user = akira::ChiakiCore::current().opened() ? activeUser() : nullptr;
    if (!user)
    {
        Snapshot snapshot = defaultSnapshotForProfile(true, false);
        {
            std::lock_guard<std::mutex> lock(mutex);
            entries[profileId].snapshot = snapshot;
        }
        if (onDone)
            brls::sync([onDone, snapshot]() { onDone(snapshot); });
        return;
    }

    uint64_t generation = 0;
    std::optional<platform::StopToken> started;
    std::optional<Snapshot> fresh;
    {
        std::lock_guard<std::mutex> lock(mutex);
        Entry& entry = entries[profileId];
        if (!force && !entry.refresh && entry.fetchedAt &&
            std::chrono::steady_clock::now() - *entry.fetchedAt < freshFor)
        {
            fresh = entry.snapshot;
        }
        else if (entry.refresh && !force)
        {
            entry.refresh->waiters.push_back({std::move(onDone), onFetch});
            generation = entry.refresh->generation;
        }
        else
        {
            std::vector<Waiter> waiters;
            if (entry.refresh)
            {
                static_cast<void>(entry.refresh->stop.request_stop());
                waiters = std::move(entry.refresh->waiters);
            }
            waiters.push_back({std::move(onDone), std::move(onFetch)});
            entry.refresh.emplace(Refresh{++entry.generation, platform::StopSource(), std::move(waiters)});
            generation = entry.refresh->generation;
            started = entry.refresh->stop.token();
            if (!entry.snapshot.hasCatalog)
                entry.snapshot = defaultSnapshotForProfile(true, true);
        }
    }

    if (fresh)
    {
        brls::Logger::debug("Cloud: catalog is fresh, not refreshing");
        if (onDone)
            brls::sync([onDone = std::move(onDone), snapshot = std::move(*fresh)]() { onDone(snapshot); });
        return;
    }

    if (!started)
    {
        const catalog::FetchState state = catalogOf().fetch_state(*user);
        if (onFetch && state.is_running())
        {
            FetchProgress current = progressOf(state);
            brls::sync([this, profileId, generation, onFetch, current = std::move(current)]() {
                if (isCurrent(profileId, generation))
                    onFetch(current);
            });
        }
        return;
    }

    HttpPool::instance().submit([this, profileId, generation, user = std::move(user), force, stop = *started](HttpSession&) {
        runRefresh(profileId, generation, user, force, stop);
    });
}

bool Service::isCurrent(int64_t profileId, uint64_t generation) const
{
    std::lock_guard<std::mutex> lock(mutex);
    auto it = entries.find(profileId);
    return it != entries.end() && it->second.refresh && it->second.refresh->generation == generation;
}

auto Service::fetchCallbacksOf(int64_t profileId, uint64_t generation) const -> std::vector<FetchCallback>
{
    std::lock_guard<std::mutex> lock(mutex);
    auto it = entries.find(profileId);
    if (it == entries.end() || !it->second.refresh || it->second.refresh->generation != generation)
        return {};
    std::vector<FetchCallback> callbacks;
    for (const Waiter& waiter : it->second.refresh->waiters)
        if (waiter.onFetch)
            callbacks.push_back(waiter.onFetch);
    return callbacks;
}

void Service::runRefresh(int64_t profileId, uint64_t generation, User user, bool force, platform::StopToken stop)
{
    std::optional<uint64_t> superseded;
    if (force)
    {
        catalogOf().start_over(*user);
        if (const catalog::FetchState running = catalogOf().fetch_state(*user); running.is_running())
            superseded = running.attempt();
    }

    const auto membership = user->get_my_membership();
    const PlusMembership plusMembership = membership
        ? observePlusMembership(membership->membership.is_ps_plus_member())
        : observePlusMembership(user->membership().is_ps_plus_member());

    const auto observer = [this, profileId, generation, stop, superseded](const catalog::FetchState& state) {
        FetchProgress progress = progressOf(state);
        if (!progress.begunAfter(superseded))
            return;
        brls::sync([this, profileId, generation, stop, progress = std::move(progress)]() {
            if (stop.stop_requested())
                return;
            for (const FetchCallback& callback : fetchCallbacksOf(profileId, generation))
                callback(progress);
        });
    };

    auto games = catalogOf().process_library_and_list(*user, stop, observer);
    if (stop.stop_requested())
        return;
    if (!games)
    {
        brls::Logger::warning("Cloud: catalog unanswered (reason {}, HTTP {})",
            static_cast<int>(games.error().reason()), games.error().http_status());
        Snapshot failed = snapshotOf(failureOf(games.error()));
        failed.status.plusMembership = plusMembership;
        finishRefresh(profileId, generation, std::move(failed), false);
        return;
    }

    Snapshot snapshot = snapshotOf(*games.value(), catalogOf(), *user, plusMembership);
    snapshot.recentlyPlayed = recentlyPlayedOf(*user);
    brls::Logger::info("Cloud: catalog has {} visible title(s), {} launchable",
        snapshot.catalog.games.size(), snapshot.status.gameCount);
    finishRefresh(profileId, generation, std::move(snapshot), true);
}

void Service::finishRefresh(int64_t profileId, uint64_t generation, Snapshot snapshot, bool fetched)
{
    std::vector<Waiter> waiters;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = entries.find(profileId);
        if (it == entries.end() || !it->second.refresh || it->second.refresh->generation != generation)
            return;
        Entry& entry = it->second;
        entry.plusMembership = snapshot.status.plusMembership;
        entry.snapshot = snapshot;
        entry.fetchedAt = fetched ? std::optional(std::chrono::steady_clock::now()) : std::nullopt;
        waiters = std::move(entry.refresh->waiters);
        entry.refresh.reset();
    }

    brls::sync([waiters = std::move(waiters), snapshot = std::move(snapshot)]() {
        for (const Waiter& waiter : waiters)
            if (waiter.onDone)
                waiter.onDone(snapshot);
    });
}

bool Service::signedIn() const
{
    return akira::ChiakiCore::current().opened() && activeUser() != nullptr;
}

auto Service::detailOf(const catalog::GameDetails& details) -> GameDetail
{
    GameDetail out;
    if (details.key_art)
    {
        if (const auto* layered = std::get_if<catalog::LayeredArt>(&details.key_art->choice()))
        {
            out.art = GameDetail::Art::Layered;
            out.background = layered->background.url();
            out.hero = layered->hero.url();
            out.logo = layered->logo.url();
        }
        else if (const auto* hub = std::get_if<catalog::GameHubArt>(&details.key_art->choice()))
        {
            out.art = GameDetail::Art::GameHub;
            out.background = hub->cover.url();
            if (hub->logo)
                out.logo = hub->logo->url();
        }
    }
    if (details.star_rating && details.star_rating->ratings > 0)
    {
        GameDetail::Rating rating;
        rating.average = details.star_rating->average;
        rating.averageText = details.star_rating->average_text;
        rating.count = details.star_rating->ratings;
        rating.percentByStars = details.star_rating->percent_by_stars;
        out.rating = rating;
    }
    if (details.age_rating && (!details.age_rating->name.empty() || details.age_rating->icon))
    {
        GameDetail::Age age;
        age.name = details.age_rating->name;
        std::ranges::replace(age.name, '_', ' ');
        for (const std::string& descriptor : details.age_rating->descriptors)
            age.descriptors += (age.descriptors.empty() ? "" : ", ") + descriptor;
        if (details.age_rating->icon)
            age.icon = details.age_rating->icon->url();
        out.age = age;
    }
    out.publisher = details.publisher;
    if (details.released_at)
        out.releasedMs = details.released_at->time_since_epoch().count();
    out.genres = details.genres;
    return out;
}

void Service::detailsOf(const Game& game, const platform::StopToken& stop, DetailCallback onDone)
{
    User user = signedIn() ? activeUser() : nullptr;
    const auto product = catalog::ProductId::parse(game.productId);
    if (!user || !product)
    {
        brls::sync([onDone]() { onDone(std::nullopt); });
        return;
    }
    HttpPool::instance().submit([user = std::move(user), product = *product, stop, onDone](HttpSession&) {
        auto details = catalogOf().details_of(*user, product, stop);
        if (stop.stop_requested())
            return;
        std::optional<GameDetail> detail;
        if (details)
            detail = detailOf(*details);
        else
            brls::Logger::warning("Cloud: details of {} unanswered (reason {})", product.text(),
                static_cast<int>(details.error().reason()));
        brls::sync([onDone, stop, detail = std::move(detail)]() {
            if (!stop.stop_requested())
                onDone(detail);
        });
    });
}

auto Service::sizedImage(const std::string& url, int width) -> std::optional<catalog::ImageRef>
{
    if (width > 0 && url.find('?') == std::string::npos && url.starts_with("https://image.api.playstation.com/"))
        if (auto sized = catalog::ImageRef::at(url + "?w=" + std::to_string(width)))
            return sized;
    return catalog::ImageRef::at(url);
}

void Service::imageOf(const std::string& url, int width, const platform::StopToken& stop, ImageCallback onDone)
{
    auto image = sizedImage(url, width);
    if (!image || !akira::ChiakiCore::current().opened())
    {
        brls::sync([onDone]() { onDone({}); });
        return;
    }
    HttpPool::instance().submit([image = *image, stop, onDone](HttpSession&) {
        auto bytes = catalogOf().image_of(image, stop);
        if (stop.stop_requested())
            return;
        std::vector<uint8_t> out;
        if (bytes)
        {
            out.resize(bytes->size());
            std::ranges::transform(*bytes, out.begin(), [](std::byte b) { return std::to_integer<uint8_t>(b); });
        }
        else
            brls::Logger::warning("Cloud: image {} unanswered", image.url());
        brls::sync([onDone, stop, out = std::move(out)]() mutable {
            if (!stop.stop_requested())
                onDone(std::move(out));
        });
    });
}

void Service::checkStorefront(const Game& game, const platform::StopToken& stop, CardCheckCallback onDone)
{
    User user = signedIn() ? activeUser() : nullptr;
    const auto product = catalog::ProductId::parse(game.productId);
    if (!user || !product)
    {
        brls::sync([onDone]() { onDone(CardCheck::unavailable("akira/cloud/check_no_sign_in"_i18n)); });
        return;
    }
    HttpPool::instance().submit([user = std::move(user), product = *product, stop, onDone](HttpSession&) {
        auto answer = catalogOf().check_title_against_storefront(*user, product, stop);
        if (stop.stop_requested())
            return;
        CardCheck check = CardCheck::unavailable("akira/cloud/check_no_answer"_i18n);
        if (!answer)
        {
            if (answer.error().needs_sign_in())
                check = CardCheck::unavailable("akira/cloud/check_no_sign_in"_i18n);
        }
        else
        {
            switch (answer->streaming())
            {
                case catalog::StorefrontAnswer::Streaming::streams: check = CardCheck::streams(); break;
                case catalog::StorefrontAnswer::Streaming::not_streamable: check = CardCheck::notStreamable(); break;
                case catalog::StorefrontAnswer::Streaming::edition_missing:
                    check = CardCheck::unavailable("akira/cloud/check_edition_missing"_i18n);
                    break;
                case catalog::StorefrontAnswer::Streaming::no_notice:
                    check = CardCheck::unavailable("akira/cloud/check_no_notice"_i18n);
                    break;
            }
        }
        brls::sync([onDone, stop, check]() {
            if (!stop.stop_requested())
                onDone(check);
        });
    });
}

void Service::replaceGame(int64_t profileId, const Game& game)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto it = entries.find(profileId);
    if (it == entries.end())
        return;
    for (Game& listed : it->second.snapshot.catalog.games)
        if (listed.productId == game.productId)
            listed = game;
}

void Service::refreshTitle(const Game& game, const platform::StopToken& stop, TitleCallback onDone)
{
    const Profile* profile = settings->getActiveProfile();
    User user = profile && signedIn() ? activeUser() : nullptr;
    const auto product = catalog::ProductId::parse(game.productId);
    if (!user || !product)
    {
        brls::sync([onDone]() { onDone(TitleRefresh{TitleRefresh::Outcome::Failed, {}, "akira/cloud/status_relink_detail"_i18n}); });
        return;
    }
    HttpPool::instance().submit([this, profileId = profile->id, user = std::move(user), product = *product, stop,
                                 onDone](HttpSession&) {
        auto answer = catalogOf().recheck_ownership(*user, product, stop);
        if (stop.stop_requested())
            return;
        TitleRefresh refresh;
        if (!answer)
            refresh.message = answer.error().needs_sign_in() ? "akira/cloud/status_relink_detail"_i18n
                                                             : "akira/cloud/title_refresh_failed"_i18n;
        else if (const catalog::MyGame* owned = answer->game())
        {
            Game updated = gameOf(*owned, catalogOf(), *user);
            replaceGame(profileId, updated);
            refresh.outcome = TitleRefresh::Outcome::Added;
            refresh.game = std::move(updated);
        }
        else
            refresh.outcome = TitleRefresh::Outcome::NotInLibrary;
        brls::sync([onDone, stop, refresh = std::move(refresh)]() {
            if (!stop.stop_requested())
                onDone(refresh);
        });
    });
}

bool Service::launchLeftOut(std::string_view serviceType)
{
    return (AKIRA_PS5_CLOUD_LAUNCH == 0 && serviceType == "pscloud")
        || (AKIRA_PS4_CLOUD_LAUNCH == 0 && serviceType == "psnow");
}

void Service::clearForLaunch(const Game& game, const platform::StopToken& stop, ClearedCallback onCleared,
                             ErrorCallback onError, ProgressCallback onProgress)
{
    if (launchLeftOut(game.serviceType))
    {
        if (onError)
            brls::sync([onError]() { onError("akira/cloud/launch_not_built"_i18n); });
        return;
    }

    const Profile* profile = settings->getActiveProfile();
    User user = profile && akira::ChiakiCore::current().opened() ? activeUser() : nullptr;
    if (!user)
    {
        if (onError)
            brls::sync([onError]() { onError("akira/cloud/status_pair_detail"_i18n); });
        return;
    }

    const auto product = catalog::ProductId::parse(game.productId);
    if (!product)
    {
        if (onError)
            brls::sync([onError]() { onError("akira/cloud/launch_unavailable"_i18n); });
        return;
    }

    bool noPlus = false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = entries.find(profile->id);
        noPlus = it != entries.end() && it->second.plusMembership == PlusMembership::None;
    }
    if (noPlus)
    {
        if (onError)
            brls::sync([onError]() { onError("akira/cloud/status_subscription_detail"_i18n); });
        return;
    }

    HttpPool::instance().submit([this, user = std::move(user), product = *product, name = game.name,
                                 stop, onCleared, onError,
                                 onProgress](HttpSession&) {
        const auto observer = [onProgress, stop](catalog::LaunchCheck check) {
            if (onProgress)
                brls::sync([onProgress, stop, check]() {
                    if (!stop.stop_requested())
                        onProgress(check);
                });
        };

        const catalog::LaunchVerdict verdict = catalogOf().clear_for_launch(*user, product, stop, observer);
        if (stop.stop_requested())
            return;

        if (const catalog::LaunchRefusal* refused = verdict.refusal())
        {
            brls::Logger::warning("CloudLaunch: {} ({}) refused by the catalog, kind {}",
                name, product.text(), static_cast<int>(refused->kind()));
            const std::string message = CloudLaunch::clearanceText(*refused, name);
            if (onError)
                brls::sync([onError, message]() { onError(message); });
            return;
        }

        const catalog::ClearedLaunch* cleared = verdict.cleared_launch();
        if (!cleared)
        {
            brls::Logger::error("CloudLaunch: {} allowed without a cleared launch", product.text());
            if (onError)
                brls::sync([onError]() { onError("akira/cloud/launch_failed"_i18n); });
            return;
        }

        auto launch = std::make_shared<CloudLaunch>(user, *cleared, name);
        brls::Logger::info("CloudLaunch: {} cleared for launch", product.text());
        if (onCleared)
            brls::sync([onCleared, launch]() { onCleared(launch); });
    });
}

} // namespace cloud
