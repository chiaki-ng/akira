#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <arisa/composition/observable.hpp>

#include "app/notifier.hpp"
#include "app/strings.hpp"
#include "cloud/cloud_desk.hpp"
#include "cloud/fetch_progress.hpp"
#include "cloud/game_actions.hpp"
#include "cloud/launch_checks.hpp"
#include "cloud/library_listing.hpp"
#include "cloud/library_status.hpp"
#include "cloud/models.hpp"
#include "cloud/title_refresh.hpp"
#include "cloud/title_refresh_cooldown.hpp"

namespace akira::cloud::ui {

class CloudModel {
public:
    using Clock = std::chrono::steady_clock;

    static constexpr std::chrono::seconds titleRefreshCooldown{ 15 };
    static constexpr int coverWidth = 440;
    static constexpr int backdropWidth = 1280;
    static constexpr int logoWidth = 640;

    struct Code {
        std::string title;
        std::string message;
        std::string url;

        friend bool operator==(const Code&, const Code&) = default;
    };

    struct Refreshing {
        TitleRefreshCooldown::Phase phase = TitleRefreshCooldown::Phase::Idle;
        int seconds = 0;
        std::optional<TitleRefresh::Outcome> outcome;
        std::string message;

        friend bool operator==(const Refreshing&, const Refreshing&) = default;
    };

    struct Detail {
        Game game;
        std::optional<GameDetail> facts;
        std::optional<CardCheck> card;
        std::optional<LaunchChecks> checks;
        std::vector<Game> bundle;
        bool choosing = false;
        Refreshing refreshing;
        std::optional<std::string> release;
    };

    struct View {
        Status status;
        LibraryStatus library;
        bool hasCatalog = false;
        std::vector<Game> games;
        std::vector<LibraryFilter> filters;
        LibraryFilter filter = LibraryFilter::All;
        PlatformFilter platform = PlatformFilter::All;
        std::string query;
        int sort = 0;
        std::size_t listed = 0;
        std::optional<FetchProgress> fetch;
        std::vector<Game> shortcuts;
        std::optional<Detail> detail;
        std::optional<Code> code;
    };

    CloudModel(CloudDesk& desk, app::Notifier& notifier, const app::Strings& strings);
    ~CloudModel();

    CloudModel(const CloudModel&) = delete;
    CloudModel& operator=(const CloudModel&) = delete;

    auto view() const -> const ::arisa::composition::Observable<View>& { return state; }

    void appear();
    void refresh(bool force);
    void tick(Clock::time_point now);

    void pickFilter(LibraryFilter filter);
    void pickPlatform(PlatformFilter platform);
    void search(std::string text);
    void sortBy(int sort);
    void clearCache();
    void signIn();
    void toggleShortcut(const Game& game);
    auto pinned(const Game& game) const -> bool;
    auto lastPlayed(const Game& game) const -> std::optional<std::int64_t>;

    void open(const Game& game);
    void close();
    void play();
    void choose(const Game& game);
    void retry(const Game& previous);
    void dismissChoice();
    void showStore(bool addingToLibrary);
    void dismissCode();
    void refreshTitle();

    auto actions() const -> std::optional<GameActions>;

    static auto statusKey(LibraryStatus::Chip chip) -> const char*;
    static auto filterKey(LibraryFilter filter) -> const char*;
    static auto sortKey(LibraryOrder order) -> const char*;
    static auto stepKey(LaunchChecks::Step step) -> const char*;
    static auto standingKey(GameActions::StandingNote note) -> const char*;

private:
    void render(const Snapshot& snapshot);
    void fetched(std::uint64_t operation, const FetchProgress& progress);
    void showCatalog(const Snapshot& snapshot);
    void applyFilter();
    void scheduleUnlock();
    void checkAndPlay(const Game& target);
    void startCardCheck();
    void loadDetails();
    void updateRefreshing(Clock::time_point now);
    void updateRelease();
    auto releaseText(const Game& game) const -> std::optional<std::string>;

    CloudDesk& desk;
    app::Notifier& notifier;
    const app::Strings& strings;
    ::arisa::composition::Observable<View> state;
    LibraryListing listing;
    FollowedFetch fetch;
    TitleRefreshCooldown cooldown{ titleRefreshCooldown };
    std::uint64_t generation = 0;
    std::uint64_t attempts = 0;
    std::optional<Clock::time_point> unlockAt;
    bool appeared = false;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

}
