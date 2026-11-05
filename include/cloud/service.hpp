#ifndef AKIRA_CLOUD_SERVICE_HPP
#define AKIRA_CLOUD_SERVICE_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <libchiaking/catalog.hpp>

#include "cloud/cloud_launch.hpp"
#include "cloud/fetch_progress.hpp"
#include "cloud/game_actions.hpp"
#include "cloud/library_listing.hpp"
#include "cloud/models.hpp"
#include "core/background_work.hpp"

class SettingsManager;

namespace cloud {

enum class Availability {
    NoProfile,
    NeedsPairing,
    Checking,
    CatalogAvailable,
    SubscriptionRequired,
    Ready,
    Warning,
    Empty,
    Error,
    LaunchBlocked
};

struct Status {
    Availability availability = Availability::NoProfile;
    std::string title;
    std::string detail;
    bool canBrowse = false;
    bool canPair = false;
    bool degraded = false;
    int gameCount = 0;
    PlusMembership plusMembership = PlusMembership::Unknown;
};

struct TitleRefresh {
    enum class Outcome {
        Added,
        NotInLibrary,
        Failed
    };

    Outcome outcome = Outcome::Failed;
    std::optional<Game> game;
    std::string message;
};

struct Snapshot {
    Status status;
    Catalog catalog;
    std::vector<RecentlyPlayed> recentlyPlayed;
    bool hasCatalog = false;
};

class Service {
public:
    using SnapshotCallback = std::function<void(const Snapshot&)>;
    using ClearedCallback = std::function<void(std::shared_ptr<CloudLaunch>)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using ProgressCallback = std::function<void(libchiaking::catalog::LaunchCheck check)>;
    using DetailCallback = std::function<void(std::optional<GameDetail>)>;
    using ImageCallback = std::function<void(std::vector<uint8_t>)>;
    using CardCheckCallback = std::function<void(CardCheck)>;
    using TitleCallback = std::function<void(TitleRefresh)>;
    using FetchCallback = std::function<void(const FetchProgress&)>;

    static Service& instance();

    Snapshot snapshotForActiveProfile() const;

    void markActiveProfileDirty();
    void clearCatalogCache();
    void refreshActiveProfile(bool force, SnapshotCallback onDone = {}, FetchCallback onFetch = {});
    void refreshInBackground();
    void clearForLaunch(const Game& game, const libchiaking::platform::StopToken& stop,
                        ClearedCallback onCleared, ErrorCallback onError, ProgressCallback onProgress);

    void detailsOf(const Game& game, const libchiaking::platform::StopToken& stop, DetailCallback onDone);
    void imageOf(const std::string& url, int width, const libchiaking::platform::StopToken& stop,
                 ImageCallback onDone);
    void checkStorefront(const Game& game, const libchiaking::platform::StopToken& stop,
                         CardCheckCallback onDone);
    void refreshTitle(const Game& game, const libchiaking::platform::StopToken& stop, TitleCallback onDone);

    std::optional<int64_t> trustedNowMs() const;
    bool signedIn() const;

    static Snapshot snapshotOf(const FetchFailure& failure);

private:
    using User = std::shared_ptr<libchiaking::users::PsUser>;

    struct Waiter {
        SnapshotCallback onDone;
        FetchCallback onFetch;
    };

    struct Refresh {
        uint64_t generation = 0;
        libchiaking::platform::StopSource stop;
        std::vector<Waiter> waiters;
    };

    struct Held {
        bool wanted = false;
        bool force = false;
        std::vector<Waiter> waiters;
    };

    struct Entry {
        Snapshot snapshot;
        std::optional<Refresh> refresh;
        std::optional<std::chrono::steady_clock::time_point> fetchedAt;
        uint64_t generation = 0;
        PlusMembership plusMembership = PlusMembership::Unknown;
    };

    static constexpr std::chrono::hours freshFor{1};
    static constexpr const char* LEGACY_CACHE_DIR = "sdmc:/switch/akira/cache/cloud";

    Service();

    static Snapshot defaultSnapshotForProfile(bool hasProfile, bool paired);
    static Snapshot snapshotOf(const libchiaking::catalog::MyGames& games, libchiaking::catalog::Catalog& catalog,
                               libchiaking::users::PsUser& user, PlusMembership plusMembership);
    static auto failureOf(const libchiaking::catalog::GamesFailure& cause) -> FetchFailure;
    static auto progressOf(const libchiaking::catalog::FetchState& state) -> FetchProgress;
    static auto stepOf(libchiaking::catalog::FetchStep step) -> FetchProgress::Step;
    static auto activityOf(libchiaking::catalog::FetchActivity activity) -> FetchProgress::Activity;
    static auto outcomeOf(libchiaking::catalog::StepOutcome outcome) -> FetchProgress::Outcome;
    static Game gameOf(const libchiaking::catalog::MyGame& game, libchiaking::catalog::Catalog& catalog,
                       libchiaking::users::PsUser& user);
    static bool launchLeftOut(std::string_view serviceType);
    static Standing standingOf(libchiaking::catalog::Standing standing);
    static auto recentlyPlayedOf(libchiaking::users::PsUser& user) -> std::vector<RecentlyPlayed>;
    static Status statusOf(const Catalog& catalog, PlusMembership plusMembership);
    static auto detailOf(const libchiaking::catalog::GameDetails& details) -> GameDetail;
    static auto sizedImage(const std::string& url, int width) -> std::optional<libchiaking::catalog::ImageRef>;
    void replaceGame(int64_t profileId, const Game& game);
    static auto catalogOf() -> libchiaking::catalog::Catalog&;

    auto activeUser() const -> User;
    void runRefresh(int64_t profileId, uint64_t generation, User user, bool force,
                    libchiaking::platform::StopToken stop);
    void finishRefresh(int64_t profileId, uint64_t generation, Snapshot snapshot, bool fetched);
    auto fetchCallbacksOf(int64_t profileId, uint64_t generation) const -> std::vector<FetchCallback>;
    bool isCurrent(int64_t profileId, uint64_t generation) const;
    void followBackgroundWork(bool paused);

    SettingsManager* settings = nullptr;

    mutable std::mutex mutex;
    std::map<int64_t, Entry> entries;
    bool paused = false;
    Held held;
    akira::BackgroundWork::Subscription backgroundSubscription;
};

} // namespace cloud

#endif // AKIRA_CLOUD_SERVICE_HPP
