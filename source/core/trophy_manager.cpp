#include "core/trophy_manager.hpp"
#include "core/chiaki_core.hpp"
#include "core/settings_manager.hpp"
#include "util/http.hpp"
#include "util/http_pool.hpp"

#include <borealis.hpp>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <format>
#include <ranges>
#include <system_error>
#include <utility>

#include <sys/stat.h>
#include <unistd.h>

namespace platform = libchiaking::platform;
namespace trophies = libchiaking::trophies;
namespace catalog = libchiaking::catalog;
using libchiaking::users::Unanswered;

TrophyManager* TrophyManager::getInstance()
{
    static TrophyManager* instance = new TrophyManager();
    return instance;
}

TrophyManager::TrophyManager()
{
    settings = SettingsManager::getInstance();
    dropLegacyCache();
}

int64_t TrophyManager::nowSeconds()
{
    return static_cast<int64_t>(std::time(nullptr));
}

void TrophyManager::dropLegacyCache()
{
    std::error_code ignored;
    std::filesystem::remove(LEGACY_RATELIMIT_PATH, ignored);

    std::vector<std::filesystem::path> legacy;
    for (std::filesystem::directory_iterator entry(TROPHY_CACHE_DIR, ignored), end; !ignored && entry != end;
         entry.increment(ignored))
    {
        if (entry->path().filename() != ICON_DIR_NAME)
            legacy.push_back(entry->path());
    }

    size_t removed = 0;
    for (const std::filesystem::path& path : legacy)
    {
        std::error_code failed;
        if (std::filesystem::remove_all(path, failed) > 0)
            removed++;
    }
    if (removed > 0)
        brls::Logger::info("Trophy: removed {} legacy JSON cache entr(ies)", removed);
}

const char* TrophyManager::reasonName(Unanswered::Reason reason)
{
    switch (reason)
    {
        case Unanswered::Reason::signed_out: return "signed out";
        case Unanswered::Reason::sign_in_expired: return "sign-in expired";
        case Unanswered::Reason::store_access_lapsed: return "store access lapsed";
        case Unanswered::Reason::remote_play_access_missing: return "remote play access missing";
        case Unanswered::Reason::cancelled: return "cancelled";
        case Unanswered::Reason::rate_limited: return "rate limited";
        case Unanswered::Reason::unavailable: return "unavailable";
        case Unanswered::Reason::rejected: return "rejected";
        case Unanswered::Reason::malformed: return "malformed reply";
        case Unanswered::Reason::old_service_refused: return "old service refused";
    }
    return "unknown";
}

psn::Error TrophyManager::failureOf(const Unanswered& cause)
{
    std::string message = cause.http_status != 0
        ? std::format("{} (HTTP {})", reasonName(cause.reason), cause.http_status)
        : std::string(reasonName(cause.reason));

    switch (cause.reason)
    {
        case Unanswered::Reason::signed_out:
            return {psn::Status::NotLinked, message};
        case Unanswered::Reason::sign_in_expired:
        case Unanswered::Reason::store_access_lapsed:
        case Unanswered::Reason::remote_play_access_missing:
            return {psn::Status::SessionExpired, message};
        case Unanswered::Reason::rate_limited:
            return {psn::Status::RateLimited, message};
        case Unanswered::Reason::unavailable:
            return {cause.http_status == 0 ? psn::Status::Offline : psn::Status::ServerError, message};
        case Unanswered::Reason::cancelled:
        case Unanswered::Reason::rejected:
        case Unanswered::Reason::malformed:
        case Unanswered::Reason::old_service_refused:
            break;
    }
    return {psn::Status::ServerError, message};
}

void TrophyManager::deliverNotLinked(const ErrorCallback& onError)
{
    brls::sync([onError]() {
        if (onError)
            onError(psn::Status::NotLinked, "PSN account not linked");
    });
}

auto TrophyManager::trophies() -> trophies::Trophies&
{
    return akira::ChiakiCore::current().chiaki().trophies();
}

auto TrophyManager::catalog() -> catalog::Catalog&
{
    return akira::ChiakiCore::current().chiaki().catalog();
}

auto TrophyManager::requestFor(psn::Accounts::User user) -> Request
{
    std::lock_guard<std::mutex> lock(mutex);
    return Request{std::move(user), generation, stopSource->token()};
}

auto TrophyManager::begin() -> std::optional<Request>
{
    psn::Accounts* accounts = akira::ChiakiCore::current().accounts();
    psn::Accounts::User user = accounts ? accounts->activeUser() : nullptr;
    if (!user || !user->has_psn_account())
        return std::nullopt;
    return requestFor(std::move(user));
}

auto TrophyManager::current(const Request& request) const -> bool
{
    std::lock_guard<std::mutex> lock(mutex);
    return request.generation == generation;
}

template <typename Value>
void TrophyManager::deliver(const Request& request, Value value, Callback<Value> onSuccess) const
{
    const uint64_t asked = request.generation;
    brls::sync([this, asked, value = std::move(value), onSuccess = std::move(onSuccess)]() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (asked != generation)
                return;
        }
        if (onSuccess)
            onSuccess(value);
    });
}

void TrophyManager::deliverFailure(const Request& request, const Unanswered& cause, const char* what,
                                   ErrorCallback onError) const
{
    if (cause.is_cancelled() || !current(request))
        return;

    const psn::Error error = failureOf(cause);
    brls::Logger::error("Trophy: {} failed with {} ({})", what, psn::statusName(error.status), error.message);

    const uint64_t asked = request.generation;
    brls::sync([this, asked, error, onError = std::move(onError)]() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (asked != generation)
                return;
        }
        if (onError)
            onError(error.status, error.message);
    });
}

void TrophyManager::flushCache()
{
    if (auto request = begin())
        trophies().start_over(*request->user);
    std::error_code ignored;
    std::filesystem::remove_all(ICON_CACHE_DIR, ignored);
    iconCacheDirReady.store(false);
    {
        std::lock_guard<std::mutex> lock(iconMutex);
        iconCache.clear();
        iconOrder.clear();
        iconCacheBytes = 0;
    }
    onActiveProfileChanged();
    brls::Logger::info("Trophy: cache flushed");
}

void TrophyManager::onActiveProfileChanged()
{
    std::shared_ptr<platform::StopSource> previous;
    {
        std::lock_guard<std::mutex> lock(mutex);
        generation++;
        librarySavedAt = 0;
        forcedAt.clear();
        playedGames.clear();
        previous = std::exchange(stopSource, std::make_shared<platform::StopSource>());
    }
    static_cast<void>(previous->request_stop());
    brls::Logger::info("Trophy: active profile changed, in-flight requests dropped");
}

void TrophyManager::ensureIconCacheDir()
{
    if (iconCacheDirReady.load())
        return;

    mkdir(CACHE_DIR, 0755);
    mkdir(TROPHY_CACHE_DIR, 0755);
    mkdir(ICON_CACHE_DIR, 0755);

    iconCacheDirReady.store(true);
}

int64_t TrophyManager::librarySavedAtSeconds() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return librarySavedAt;
}

psn::ActionStatus TrophyManager::forceRefreshStatus(const std::string& scope)
{
    std::lock_guard<std::mutex> lock(mutex);

    auto entry = forcedAt.find(scope);
    if (entry == forcedAt.end() || entry->second <= 0)
        return {psn::ActionState::Ready, 0};

    const int64_t now = nowSeconds();
    if (entry->second > now)
        entry->second = now;

    const int64_t readyAt = entry->second + static_cast<int64_t>(FORCE_REFRESH_COOLDOWN_MINUTES) * 60;
    if (readyAt <= now)
        return {psn::ActionState::Ready, 0};

    return {psn::ActionState::CoolingDown, static_cast<int>(readyAt - now)};
}

void TrophyManager::recordForcedRefresh(const std::string& scope)
{
    std::lock_guard<std::mutex> lock(mutex);
    forcedAt[scope] = nowSeconds();

    brls::Logger::info("Trophy: force refresh of '{}' recorded, next available in {} min",
        scope, FORCE_REFRESH_COOLDOWN_MINUTES);
}

std::string TrophyManager::iconCachePath(const std::string& url) const
{
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char c : url)
    {
        hash ^= c;
        hash *= 1099511628211ULL;
    }

    return std::format("{}/{:016x}.img", ICON_CACHE_DIR, hash);
}

bool TrophyManager::readIconFromDisk(const std::string& path, std::vector<uint8_t>& outBytes) const
{
    FILE* file = fopen(path.c_str(), "rb");
    if (!file)
        return false;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size <= 0 || static_cast<size_t>(size) > ICON_MAX_BYTES)
    {
        fclose(file);
        return false;
    }

    outBytes.resize(static_cast<size_t>(size));
    size_t read = fread(outBytes.data(), 1, outBytes.size(), file);
    fclose(file);

    if (read != outBytes.size())
    {
        outBytes.clear();
        return false;
    }

    return true;
}

void TrophyManager::writeIconToDisk(const std::string& path, const std::vector<uint8_t>& bytes)
{
    std::string temp = path + ".tmp";

    FILE* file = fopen(temp.c_str(), "wb");
    if (!file)
    {
        stopIconDiskWrites(std::format("could not open {}: {}", temp, strerror(errno)));
        return;
    }

    size_t written = fwrite(bytes.data(), 1, bytes.size(), file);
    fclose(file);

    if (written != bytes.size())
    {
        remove(temp.c_str());
        stopIconDiskWrites(std::format("short write to {} ({}/{} bytes)", temp, written, bytes.size()));
        return;
    }

    remove(path.c_str());
    if (rename(temp.c_str(), path.c_str()) != 0)
    {
        remove(temp.c_str());
        stopIconDiskWrites(std::format("could not replace {}", path));
    }
}

void TrophyManager::stopIconDiskWrites(const std::string& reason)
{
    if (!iconDiskWritable.exchange(false))
        return;

    brls::Logger::error("Trophy: icon disk cache disabled for this session ({}); icons will still load from memory", reason);
}

void TrophyManager::storeIconInMemory(const std::string& url, const std::vector<uint8_t>& bytes)
{
    std::lock_guard<std::mutex> lock(iconMutex);

    if (iconCache.find(url) == iconCache.end())
    {
        iconCacheBytes += bytes.size();
        iconCache[url] = bytes;
        iconOrder.push_back(url);
    }

    while (iconCacheBytes > ICON_CACHE_MAX_BYTES && !iconOrder.empty())
    {
        const std::string oldest = iconOrder.front();
        iconOrder.pop_front();

        if (oldest == url)
            continue;

        auto entry = iconCache.find(oldest);
        if (entry != iconCache.end())
        {
            iconCacheBytes -= entry->second.size();
            iconCache.erase(entry);
        }
    }
}

void TrophyManager::logLibrary(const std::vector<Title>& titles) const
{
    int logged = 0;

    for (const Title& title : titles)
    {
        if (logged >= LIBRARY_LOG_CAP)
        {
            brls::Logger::info("Trophy library: {} further title(s) not logged",
                titles.size() - static_cast<size_t>(logged));
            break;
        }

        brls::Logger::info("  [{}] {} '{}' {}% {}/{} (P{}/{} G{}/{} S{}/{} B{}/{}) groups={}({}) hidden={} v{}",
            logged, title.id.text(), title.name, title.progress,
            title.earned.total(), title.defined.total(),
            title.earned.platinum, title.defined.platinum,
            title.earned.gold, title.defined.gold,
            title.earned.silver, title.defined.silver,
            title.earned.bronze, title.defined.bronze,
            title.has_groups ? "yes" : "no", title.group_count,
            title.hidden ? "yes" : "no", title.set_version);

        logged++;
    }
}

void TrophyManager::fetchSummary(bool forceRefresh, Callback<Summary> onSuccess, ErrorCallback onError)
{
    std::optional<Request> pending = begin();
    if (!pending)
    {
        deliverNotLinked(onError);
        return;
    }

    HttpPool::instance().submit([this, request = std::move(*pending), forceRefresh, onSuccess, onError](HttpSession&) {
        if (!current(request))
            return;
        if (forceRefresh)
            trophies().start_over(*request.user);

        auto summary = trophies().summary_of(*request.user, request.stop);
        if (!summary)
        {
            deliverFailure(request, summary.error(), "summary fetch", onError);
            return;
        }

        brls::Logger::info("Trophy summary: level {} tier {} progress {}% earned {} (P{} G{} S{} B{})",
            summary->level, summary->tier, summary->progress, summary->earned.total(),
            summary->earned.platinum, summary->earned.gold, summary->earned.silver, summary->earned.bronze);

        deliver(request, std::move(summary).value(), onSuccess);
    });
}

void TrophyManager::fetchProfile(bool forceRefresh, Callback<Profile> onSuccess, ErrorCallback onError)
{
    static_cast<void>(forceRefresh);

    std::optional<Request> pending = begin();
    if (!pending)
    {
        deliverNotLinked(onError);
        return;
    }

    HttpPool::instance().submit([this, request = std::move(*pending), onSuccess, onError](HttpSession&) {
        if (!current(request))
            return;

        auto profile = request.user->get_my_profile(request.stop);
        if (!profile)
        {
            deliverFailure(request, profile.error(), "profile fetch", onError);
            return;
        }

        brls::Logger::info("PSN profile: {} plus={} avatar={}", profile->online_id,
            profile->membership.is_ps_plus_member().value_or(false), profile->avatar_url.has_value());

        deliver(request, std::move(profile).value(), onSuccess);
    });
}

void TrophyManager::fetchProfileForAccount(const std::string& accountId,
    Callback<Profile> onSuccess, ErrorCallback onError)
{
    akira::ChiakiCore& core = akira::ChiakiCore::current();
    const auto account = libchiaking::AccountId::from_base64(accountId);
    psn::Accounts::User user = core.opened() && account ? core.chiaki().users().find(*account) : nullptr;
    if (!user || !user->has_psn_account())
    {
        deliverNotLinked(onError);
        return;
    }

    HttpPool::instance().submit([this, request = requestFor(std::move(user)), onSuccess, onError](HttpSession&) {
        if (!current(request))
            return;

        auto profile = request.user->get_my_profile(request.stop);
        if (!profile)
        {
            deliverFailure(request, profile.error(), "profile lookup", onError);
            return;
        }
        deliver(request, std::move(profile).value(), onSuccess);
    });
}

void TrophyManager::fetchLibrary(bool forceRefresh, Callback<std::vector<Title>> onSuccess, ErrorCallback onError)
{
    std::optional<Request> pending = begin();
    if (!pending)
    {
        deliverNotLinked(onError);
        return;
    }

    HttpPool::instance().submit([this, request = std::move(*pending), forceRefresh, onSuccess, onError](HttpSession&) {
        if (!current(request))
            return;
        if (forceRefresh)
            trophies().start_over(*request.user);

        auto titles = trophies().titles_of(*request.user, request.stop);
        if (!titles)
        {
            deliverFailure(request, titles.error(), "library fetch", onError);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            if (request.generation == generation)
                librarySavedAt = nowSeconds();
        }

        logLibrary(*titles);
        prefetchIcons(*titles);

        deliver(request, std::move(titles).value(), onSuccess);
    });
}

void TrophyManager::fetchTitleDetail(const Title& title, bool forceRefresh,
    Callback<TitleTrophies> onSuccess, ErrorCallback onError)
{
    std::optional<Request> pending = begin();
    if (!pending)
    {
        deliverNotLinked(onError);
        return;
    }

    HttpPool::instance().submit([this, request = std::move(*pending), title, forceRefresh, onSuccess, onError](HttpSession&) {
        if (!current(request))
            return;
        if (forceRefresh)
            trophies().start_over(*request.user);

        auto detail = trophies().trophies_of(*request.user, title, request.stop);
        if (!detail)
        {
            deliverFailure(request, detail.error(), "detail fetch", onError);
            return;
        }

        brls::Logger::info("Trophy detail {}: {} trophies across {} group(s)",
            title.id.text(), detail->trophies.size(), detail->groups.size());

        deliver(request, std::move(detail).value(), onSuccess);
    });
}

void TrophyManager::resolvePlayedGames(const std::vector<Title>& titles, std::function<void()> onUpdated)
{
    std::set<trophies::NpCommunicationId> wanted;
    for (const Title& title : titles)
        wanted.insert(title.id);

    std::optional<Request> pending = begin();
    if (!pending || wanted.empty())
        return;

    HttpPool::instance().submit([this, request = std::move(*pending), wanted = std::move(wanted),
                                    onUpdated = std::move(onUpdated)](HttpSession&) mutable {
        if (!current(request))
            return;

        auto games = catalog().played_games(*request.user, request.stop);
        if (!games)
        {
            const catalog::PlayedGamesFailure& cause = games.error();
            if (!cause.is_cancelled() && current(request))
            {
                brls::Logger::warning("Trophy: play history unavailable ({}, HTTP {})",
                    cause.cause() ? reasonName(cause.cause()->reason) : "catalog refused",
                    cause.http_status());
            }
            return;
        }

        const size_t asked = wanted.size();
        auto joined = joinPlayedGames(request, *games, std::move(wanted));
        const size_t resolved = joined.size();

        {
            std::lock_guard<std::mutex> lock(mutex);
            if (request.generation != generation)
                return;
            playedGames = std::move(joined);
        }

        brls::Logger::info("Trophy: joined {} played game(s) to {} of {} trophy title(s)",
            games->size(), resolved, asked);

        const uint64_t generationAsked = request.generation;
        brls::sync([this, generationAsked, onUpdated = std::move(onUpdated)]() {
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (generationAsked != generation)
                    return;
            }
            if (onUpdated)
                onUpdated();
        });
    });
}

auto TrophyManager::joinPlayedGames(const Request& request, const std::vector<PlayedGame>& games,
    std::set<trophies::NpCommunicationId> wanted) -> std::map<trophies::NpCommunicationId, PlayedGame>
{
    std::map<trophies::NpCommunicationId, PlayedGame> joined;

    for (auto chunk : games | std::views::chunk(trophies::Trophies::titles_per_request))
    {
        if (wanted.empty())
            break;

        const auto releases = chunk
            | std::views::transform(&PlayedGame::title)
            | std::ranges::to<std::vector>();

        auto sets = trophies().titles_for(*request.user, releases, request.stop);
        if (!sets)
        {
            if (!sets.error().is_cancelled())
                brls::Logger::warning("Trophy: title mapping failed ({})", reasonName(sets.error().reason));
            break;
        }

        for (const trophies::TitleSets& entry : *sets)
        {
            auto game = std::ranges::find(chunk, entry.title, &PlayedGame::title);
            if (game == chunk.end())
                continue;

            for (const trophies::NpCommunicationId& set : entry.sets)
            {
                if (wanted.erase(set) > 0)
                    joined.emplace(set, *game);
            }
        }
    }

    return joined;
}

auto TrophyManager::playedGameFor(const trophies::NpCommunicationId& title) const -> std::optional<PlayedGame>
{
    std::lock_guard<std::mutex> lock(mutex);
    auto entry = playedGames.find(title);
    if (entry == playedGames.end())
        return std::nullopt;
    return entry->second;
}

void TrophyManager::fetchIcon(const std::string& url, IconCallback onSuccess)
{
    if (url.empty())
        return;

    {
        std::lock_guard<std::mutex> lock(iconMutex);

        auto cached = iconCache.find(url);
        if (cached != iconCache.end())
        {
            if (onSuccess)
            {
                std::vector<uint8_t> bytes = cached->second;
                brls::sync([onSuccess, url, bytes]() { onSuccess(url, bytes); });
            }
            return;
        }

        auto existing = iconWaiters.find(url);
        bool alreadyInFlight = existing != iconWaiters.end();

        if (onSuccess)
            iconWaiters[url].push_back(std::move(onSuccess));
        else if (!alreadyInFlight)
            iconWaiters[url];

        if (alreadyInFlight)
            return;
    }

    HttpPool::instance().submit([this, url](HttpSession& session) {
        ensureIconCacheDir();

        std::string path = iconCachePath(url);
        std::vector<uint8_t> bytes;
        bool usable = readIconFromDisk(path, bytes);
        bool fromDisk = usable;

        if (!usable)
        {
            HttpResponse response = session.get(url, "", ICON_TIMEOUT_S);

            if (response.transportFailed())
            {
                brls::Logger::warning("Trophy icon fetch failed ({}): {}", url, response.error);
            }
            else if (response.status != 200)
            {
                brls::Logger::warning("Trophy icon fetch returned HTTP {} for {}", response.status, url);
            }
            else if (response.body.size() > ICON_MAX_BYTES)
            {
                brls::Logger::warning("Trophy icon {} is {} bytes, over the {} byte cap, dropping",
                    url, response.body.size(), ICON_MAX_BYTES);
            }
            else if (response.body.empty())
            {
                brls::Logger::warning("Trophy icon {} came back empty", url);
            }
            else
            {
                bytes.assign(response.body.begin(), response.body.end());
                usable = true;
            }
        }

        if (usable)
            storeIconInMemory(url, bytes);

        std::vector<IconCallback> waiters;
        {
            std::lock_guard<std::mutex> lock(iconMutex);
            auto entry = iconWaiters.find(url);
            if (entry != iconWaiters.end())
            {
                waiters.swap(entry->second);
                iconWaiters.erase(entry);
            }
        }

        if (!usable)
            return;

        if (!fromDisk && iconDiskWritable.load())
            writeIconToDisk(path, bytes);

        if (waiters.empty())
            return;

        brls::sync([waiters, url, bytes]() {
            for (const IconCallback& waiter : waiters)
                waiter(url, bytes);
        });
    });
}

void TrophyManager::discardIcon(const std::string& url)
{
    {
        std::lock_guard<std::mutex> lock(iconMutex);

        auto entry = iconCache.find(url);
        if (entry != iconCache.end())
        {
            iconCacheBytes -= entry->second.size();
            iconCache.erase(entry);
        }

        auto position = std::find(iconOrder.begin(), iconOrder.end(), url);
        if (position != iconOrder.end())
            iconOrder.erase(position);
    }

    remove(iconCachePath(url).c_str());
    brls::Logger::warning("Trophy: discarded the cached icon for {}", url);
}

void TrophyManager::setSummaryObserver(Callback<Summary> observer)
{
    summaryObserver = std::move(observer);
}

void TrophyManager::setLibraryObserver(Callback<std::vector<Title>> observer)
{
    libraryObserver = std::move(observer);
}

void TrophyManager::startAutoRefresh()
{
    if (autoRefreshStarted)
        return;

    autoRefreshStarted = true;

    staleTimer.setCallback([this]() { runStaleCheck(); });
    staleTimer.start(STALE_CHECK_MINUTES * 60 * 1000);

    brls::Logger::info("Trophy: auto refresh checking every {} min", STALE_CHECK_MINUTES);
}

void TrophyManager::runStaleCheck()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        const int64_t keptFor = std::chrono::duration_cast<std::chrono::seconds>(trophies::Trophies::kept_for).count();
        if (librarySavedAt == 0 || nowSeconds() - librarySavedAt < keptFor)
            return;
    }

    brls::Logger::info("Trophy: titles passed the core's cache lifetime, refreshing in the background");

    fetchSummary(false,
        [this](const Summary& summary) {
            if (summaryObserver)
                summaryObserver(summary);
        },
        [](psn::Status status, const std::string& message) {
            brls::Logger::warning("Trophy: background summary refresh failed [{}] {}",
                psn::statusName(status), message);
        });

    fetchLibrary(false,
        [this](const std::vector<Title>& titles) {
            if (libraryObserver)
                libraryObserver(titles);
        },
        [](psn::Status status, const std::string& message) {
            brls::Logger::warning("Trophy: background library refresh failed [{}] {}",
                psn::statusName(status), message);
        });
}

void TrophyManager::prefetchIcons(const std::vector<Title>& titles)
{
    int queued = 0;
    int skipped = 0;

    for (const Title& title : titles)
    {
        if (title.icon_url.empty())
            continue;

        if (queued >= ICON_PREFETCH_CAP)
        {
            skipped++;
            continue;
        }

        queued++;
        fetchIcon(title.icon_url, nullptr);
    }

    if (skipped > 0)
    {
        brls::Logger::info("Trophy: prefetching {} icon(s), {} beyond the cap left to load on scroll",
            queued, skipped);
    }
    else if (queued > 0)
    {
        brls::Logger::info("Trophy: prefetching {} icon(s) into the disk cache", queued);
    }
}
