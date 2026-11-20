#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <arisa/composition/observable.hpp>

#include "accounts/status.hpp"
#include "app/notifier.hpp"
#include "app/strings.hpp"
#include "trophies/title_listing.hpp"
#include "trophies/trophy_desk.hpp"
#include "trophies/trophy_listing.hpp"

namespace akira::trophies::ui {

class TrophyModel {
public:
    using Clock = std::chrono::steady_clock;
    using Title = TrophyDesk::Title;
    using Summary = TrophyDesk::Summary;
    using Detail = TrophyDesk::Detail;
    using Trophy = TrophyListing::Trophy;
    using Group = TrophyListing::Group;
    using Counts = libchiaking::trophies::TrophyCounts;

    enum class Phase { Loading, Ready, Failed, NotLinked, Disabled };

    struct Row {
        Title title;
        std::string played;

        friend bool operator==(const Row&, const Row&) = default;
    };

    struct Page {
        Title title;
        bool loading = true;
        std::string error;
        std::vector<Group> groups;
        std::vector<Trophy> trophies;
        Counts earned;
        Counts defined;
        std::uint32_t progress = 0;
        std::set<std::uint32_t> revealed;
        std::optional<Trophy> shown;
        std::optional<int> wait;
        bool refreshing = false;

        friend bool operator==(const Page&, const Page&) = default;
    };

    struct View {
        Phase phase = Phase::Loading;
        std::optional<Summary> summary;
        bool summaryFailed = false;
        std::vector<Row> rows;
        bool anyTitles = false;
        TitleListing listing;
        TrophyListing trophyListing;
        std::string updated;
        std::string error;
        bool refreshing = false;
        std::optional<int> wait;
        std::optional<Page> page;

        friend bool operator==(const View&, const View&) = default;
    };

    static constexpr std::chrono::seconds updatedEvery{ 30 };

    TrophyModel(TrophyDesk& desk, app::Notifier& notifier, const app::Strings& strings);
    ~TrophyModel();

    TrophyModel(const TrophyModel&) = delete;
    TrophyModel& operator=(const TrophyModel&) = delete;

    auto view() const -> const ::arisa::composition::Observable<View>& { return state; }

    void appear();
    void reset();
    void tick(Clock::time_point now);
    void refresh();
    void retry();
    void setSort(TitleListing::Sort sort);
    void setFilter(TitleListing::Filter filter);

    void open(const std::string& titleId);
    void close();
    void selectGroup(std::optional<std::string> groupId);
    void setTrophySort(TrophyListing::Sort sort);
    void setTrophyFilter(TrophyListing::Filter filter);
    void reveal(std::uint32_t trophyId);
    void refreshTitle();
    void showTrophy(std::uint32_t trophyId);
    void closeTrophy();

    auto playedText(const Title& title) const -> std::string;
    auto updatedText(std::int64_t savedAt, std::int64_t now) const -> std::string;
    auto rarityText(const Trophy& trophy) const -> std::string;
    auto earnedText(const Trophy& trophy) const -> std::string;
    auto waitText(int seconds) const -> std::string;

    static auto errorKey(accounts::Status status) -> const char*;
    static auto waitShown(int seconds) -> std::string;

private:
    void load(bool force);
    void openPage(const Title& title, bool force);
    void rebuild();
    void rebuildPage(View& next) const;
    void refreshGates(View& next);
    auto agoText(std::int64_t age) const -> std::string;
    static auto nowSeconds() -> std::int64_t;

    TrophyDesk& desk;
    app::Notifier& notifier;
    const app::Strings& strings;
    ::arisa::composition::Observable<View> state;
    std::vector<Title> titles;
    std::optional<Detail> detail;
    std::uint64_t pageGeneration = 0;
    bool requested = false;
    std::optional<Clock::time_point> stamped;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

}
