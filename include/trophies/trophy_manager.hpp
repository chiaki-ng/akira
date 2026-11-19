#ifndef AKIRA_TROPHY_MANAGER_HPP
#define AKIRA_TROPHY_MANAGER_HPP

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <libchiaking/catalog.hpp>
#include <libchiaking/trophies.hpp>
#include <libchiaking/users.hpp>

#include "accounts/accounts.hpp"
#include "accounts/status.hpp"
#include "app/ui_dispatcher.hpp"
#include "trophies/icon_cache.hpp"

class HttpPool;

namespace akira::play {
class ChiakiCore;
}

namespace akira::trophies {

class TrophyManager {
public:
    template <typename T>
    using Callback = std::function<void(const T&)>;
    using ErrorCallback = std::function<void(accounts::Status, const std::string&)>;
    using IconCallback = std::function<void(const std::string& url, const std::vector<uint8_t>&)>;

    using Summary = libchiaking::trophies::TrophySummary;
    using Title = libchiaking::trophies::TrophyTitle;
    using TitleTrophies = libchiaking::trophies::TitleTrophies;
    using Profile = libchiaking::users::Profile;
    using PlayedGame = libchiaking::catalog::PlayedGame;

    TrophyManager(HttpPool& http, app::UiDispatcher& dispatcher, play::ChiakiCore& core);

    TrophyManager(const TrophyManager&) = delete;
    TrophyManager& operator=(const TrophyManager&) = delete;

    void fetchSummary(bool forceRefresh, Callback<Summary> onSuccess, ErrorCallback onError);
    void fetchProfile(bool forceRefresh, Callback<Profile> onSuccess, ErrorCallback onError);
    void fetchProfileForAccount(const std::string& accountId, Callback<Profile> onSuccess, ErrorCallback onError);
    void fetchLibrary(bool forceRefresh, Callback<std::vector<Title>> onSuccess, ErrorCallback onError);
    void fetchTitleDetail(const Title& title, bool forceRefresh, Callback<TitleTrophies> onSuccess,
                          ErrorCallback onError);

    void resolvePlayedGames(const std::vector<Title>& titles, std::function<void()> onUpdated);
    std::optional<PlayedGame> playedGameFor(const libchiaking::trophies::NpCommunicationId& title) const;

    void fetchIcon(const std::string& url, IconCallback onSuccess);
    void discardIcon(const std::string& url);

    void onActiveProfileChanged();
    void flushCache();

    void startAutoRefresh();

    void setSummaryObserver(Callback<Summary> observer);
    void setLibraryObserver(Callback<std::vector<Title>> observer);

    int64_t librarySavedAtSeconds() const;

    static constexpr const char* SCOPE_LIBRARY = "library";

    accounts::ActionStatus forceRefreshStatus(const std::string& scope = SCOPE_LIBRARY);
    void recordForcedRefresh(const std::string& scope = SCOPE_LIBRARY);

private:
    struct Request {
        accounts::Accounts::User user;
        uint64_t generation = 0;
        libchiaking::platform::StopToken stop;
    };

    static constexpr int STALE_CHECK_MINUTES = 5;
    static constexpr int FORCE_REFRESH_COOLDOWN_MINUTES = 360;
    static constexpr int LIBRARY_LOG_CAP = 50;
    static constexpr int ICON_PREFETCH_CAP = 120;
    static constexpr long ICON_TIMEOUT_S = 20;
    static constexpr size_t ICON_CACHE_MAX_BYTES = 12 * 1024 * 1024;
    static constexpr size_t ICON_MAX_BYTES = 2 * 1024 * 1024;
    static constexpr const char* TROPHY_CACHE_DIR = "sdmc:/switch/akira/cache/trophies";
    static constexpr const char* ICON_DIR_NAME = "icons";
    static constexpr const char* ICON_CACHE_DIR = "sdmc:/switch/akira/cache/trophies/icons";
    static constexpr const char* LEGACY_RATELIMIT_PATH = "sdmc:/switch/akira/cache/ratelimit.json";

    static accounts::Error failureOf(const libchiaking::users::Unanswered& cause);
    static const char* reasonName(libchiaking::users::Unanswered::Reason reason);
    static int64_t nowSeconds();
    void deliverNotLinked(const ErrorCallback& onError) const;

    auto begin() -> std::optional<Request>;
    auto requestFor(accounts::Accounts::User user) -> Request;
    auto current(const Request& request) const -> bool;
    template <typename Value>
    void deliver(const Request& request, Value value, Callback<Value> onSuccess) const;
    void deliverFailure(const Request& request, const libchiaking::users::Unanswered& cause, const char* what,
                        ErrorCallback onError) const;
    auto trophies() -> libchiaking::trophies::Trophies&;
    auto catalog() -> libchiaking::catalog::Catalog&;
    auto joinPlayedGames(const Request& request, const std::vector<PlayedGame>& games,
                         std::set<libchiaking::trophies::NpCommunicationId> wanted)
        -> std::map<libchiaking::trophies::NpCommunicationId, PlayedGame>;

    void dropLegacyCache();
    void prefetchIcons(const std::vector<Title>& titles);
    void logLibrary(const std::vector<Title>& titles) const;
    void runStaleCheck();

    HttpPool& http;
    app::UiDispatcher& dispatcher;
    play::ChiakiCore& core;

    std::unique_ptr<app::UiDispatcher::Repeating> staleTimer;
    bool autoRefreshStarted = false;
    Callback<Summary> summaryObserver;
    Callback<std::vector<Title>> libraryObserver;

    mutable std::mutex mutex;
    uint64_t generation = 0;
    std::shared_ptr<libchiaking::platform::StopSource> stopSource = std::make_shared<libchiaking::platform::StopSource>();
    int64_t librarySavedAt = 0;
    std::unordered_map<std::string, int64_t> forcedAt;
    std::map<libchiaking::trophies::NpCommunicationId, PlayedGame> playedGames;

    IconCache icons{ICON_CACHE_DIR, ICON_CACHE_MAX_BYTES, ICON_MAX_BYTES};
    mutable std::mutex iconMutex;
    std::unordered_map<std::string, std::vector<IconCallback>> iconWaiters;
};

}

#endif // AKIRA_TROPHY_MANAGER_HPP
