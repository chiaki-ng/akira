#include "test_util.hpp"

#include "cloud/fetch_progress.hpp"

#include <json-c/json.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace cloud;

namespace {

using Step = FetchProgress::Step;
using Activity = FetchProgress::Activity;

FetchProgress at(uint64_t attempt, Step step, std::optional<Activity> activity = std::nullopt, uint32_t read = 0,
                 std::optional<uint32_t> expected = std::nullopt)
{
    return FetchProgress(attempt, step, activity, read, expected, {}, std::nullopt);
}

class EnglishText {
public:
    EnglishText()
        : root(json_object_from_file(
              (std::filesystem::path(__FILE__).parent_path().parent_path() / "resources/i18n/en-US/akira.json")
                  .c_str()))
    {
    }

    ~EnglishText()
    {
        json_object_put(root);
    }

    EnglishText(const EnglishText&) = delete;
    EnglishText& operator=(const EnglishText&) = delete;

    auto of(std::string_view key, const std::vector<uint32_t>& args = {}) const -> std::string
    {
        constexpr std::string_view prefix = "akira/cloud/";
        if (!root || !key.starts_with(prefix))
            return {};
        json_object* cloud = nullptr;
        json_object* entry = nullptr;
        if (!json_object_object_get_ex(root, "cloud", &cloud) ||
            !json_object_object_get_ex(cloud, std::string(key.substr(prefix.size())).c_str(), &entry))
            return {};
        std::string text = json_object_get_string(entry);
        for (uint32_t arg : args)
            if (const auto slot = text.find("{}"); slot != std::string::npos)
                text.replace(slot, 2, std::to_string(arg));
        return text;
    }

    auto lines(const FetchProgress& progress) const -> std::vector<std::string>
    {
        std::vector<std::string> out;
        if (const auto step = progress.stepKey())
            out.push_back(of(*step));
        if (const auto detail = progress.detailKey())
            out.push_back(of(*detail));
        if (const auto page = progress.pageKey())
        {
            std::vector<uint32_t> args{progress.pagesRead()};
            if (progress.pagesExpected())
                args.push_back(*progress.pagesExpected());
            out.push_back(of(*page, args));
        }
        return out;
    }

private:
    json_object* root;
};

using Lines = std::vector<std::string>;

}

TEST(fetch_states_drive_the_loading_text)
{
    const EnglishText text;
    FollowedFetch fetch;
    fetch.follow(4);

    const std::vector<std::pair<FetchProgress, Lines>> script = {
        {at(1, Step::CheckingSignIn), {"Checking your sign-in"}},
        {at(1, Step::CloudList, Activity::WalkingOldServiceShelves),
         {"Reading the cloud catalog", "PS Now shelves"}},
        {at(1, Step::CloudList, Activity::WalkingOldServiceShelves, 3),
         {"Reading the cloud catalog", "PS Now shelves", "Page 3"}},
        {at(1, Step::CloudList, Activity::WalkingPublicStoreFallback, 3, 7),
         {"Reading the cloud catalog", "PlayStation Store, as a fallback", "Page 3 of 7"}},
        {at(1, Step::OwnedLibrary, Activity::ReadingEntitlements, 2),
         {"Reading your games", "Your entitlements", "Page 2"}},
        {at(1, Step::PremiumCategory, Activity::ReusingHeldAnswer),
         {"Reading the Game Catalog", "Using what was saved earlier"}},
        {at(1, Step::ForSaleCategory, Activity::ReadingCategoryPages, 1, 4),
         {"Reading cloud games for sale", "Store pages", "Page 1 of 4"}},
        {at(1, Step::StreamingAccess, Activity::ReadingSubscriptions),
         {"Checking cloud streaming access", "Your subscriptions"}},
        {at(1, Step::Matching, Activity::MatchingGames), {"Putting your library together", "Matching titles"}},
        {at(1, Step::TrustedTime, Activity::SyncingClock), {"Checking the time", "Trusted time"}},
        {at(1, Step::Exclusions, Activity::ReadingExclusions),
         {"Checking which games are unavailable", "Exclusion list"}},
        {at(1, Step::Ready), {}},
    };

    for (const auto& [progress, expected] : script)
    {
        CHECK(fetch.observe(4, progress) != FollowedFetch::Change::Dropped);
        REQUIRE(fetch.latest().has_value());
        CHECK(text.lines(*fetch.latest()) == expected);
    }
}

TEST(fetch_page_changes_keep_the_step_and_step_changes_animate)
{
    FollowedFetch fetch;
    fetch.follow(1);
    CHECK(fetch.observe(1, at(1, Step::OwnedLibrary, Activity::ScanningPs5Library, 1, 5)) ==
          FollowedFetch::Change::Step);
    CHECK(fetch.observe(1, at(1, Step::OwnedLibrary, Activity::ScanningPs5Library, 2, 5)) ==
          FollowedFetch::Change::Lines);
    CHECK(fetch.observe(1, at(1, Step::OwnedLibrary, Activity::ScanningPs5Library, 2, 5)) ==
          FollowedFetch::Change::Unchanged);
    CHECK(fetch.observe(1, at(1, Step::PremiumCategory, Activity::ReadingCategoryPages)) ==
          FollowedFetch::Change::Step);
}

TEST(fetch_state_from_an_abandoned_fetch_is_dropped)
{
    FollowedFetch fetch;
    fetch.follow(1);
    CHECK(fetch.observe(1, at(1, Step::CloudList, Activity::WalkingPublicStore)) == FollowedFetch::Change::Step);
    fetch.follow(2);
    CHECK(!fetch.latest().has_value());
    CHECK(fetch.observe(1, at(1, Step::OwnedLibrary, Activity::ReadingEntitlements)) ==
          FollowedFetch::Change::Dropped);
    CHECK(!fetch.latest().has_value());
    CHECK(fetch.observe(2, at(2, Step::CheckingSignIn)) == FollowedFetch::Change::Step);
    CHECK(fetch.latest()->step() == Step::CheckingSignIn);
}

TEST(fetch_unanswered_goes_to_the_error_view)
{
    const EnglishText text;
    FollowedFetch fetch;
    fetch.follow(3);
    const FetchProgress failed(1, Step::Unanswered, std::nullopt, 0, std::nullopt,
                               {{Step::CheckingSignIn, FetchProgress::Outcome::Fetched}},
                               FetchFailure(FetchFailure::Reason::RegionUnsupported, 0, false));
    CHECK(fetch.observe(3, failed) == FollowedFetch::Change::Step);
    REQUIRE(fetch.latest().has_value());
    CHECK(fetch.latest()->shown() == FetchProgress::Shown::Failure);
    REQUIRE(fetch.latest()->failure().has_value());
    CHECK(fetch.latest()->failure()->reason() == FetchFailure::Reason::RegionUnsupported);
    CHECK(text.lines(*fetch.latest()).empty());
}

TEST(fetch_idle_ready_and_cancelled_show_no_fetch_line)
{
    for (Step step : {Step::Idle, Step::Ready, Step::Cancelled})
    {
        const FetchProgress progress = at(1, step, Activity::MatchingGames, 2, 3);
        CHECK(progress.shown() == FetchProgress::Shown::Nothing);
        CHECK(!progress.stepKey());
        CHECK(!progress.detailKey());
        CHECK(!progress.pageKey());
    }
}

TEST(fetch_joined_mid_way_shows_the_running_state_first)
{
    const EnglishText text;
    FollowedFetch fetch;
    fetch.follow(7);
    const FetchProgress running(2, Step::OwnedLibrary, Activity::ReadingStreamedPs4Concepts, 4, 9,
                                {{Step::CheckingSignIn, FetchProgress::Outcome::Fetched},
                                 {Step::CloudList, FetchProgress::Outcome::Held}},
                                std::nullopt);
    CHECK(fetch.observe(7, running) == FollowedFetch::Change::Step);
    CHECK(text.lines(*fetch.latest()) == (Lines{"Reading your games", "Your PS4 games", "Page 4 of 9"}));
    CHECK(fetch.latest()->passed().size() == 2u);
    CHECK(fetch.observe(7, at(2, Step::OwnedLibrary, Activity::ReadingStreamedPs4Concepts, 5, 9)) ==
          FollowedFetch::Change::Lines);
    CHECK(text.lines(*fetch.latest()) == (Lines{"Reading your games", "Your PS4 games", "Page 5 of 9"}));
}

TEST(fetch_trusted_time_names_the_server_it_asks)
{
    const EnglishText text;
    const FetchProgress asking(1, Step::TrustedTime, Activity::SyncingClock, 0, std::nullopt, {}, std::nullopt,
                               "time.cloudflare.com");
    REQUIRE(asking.detailKey().has_value());
    CHECK(text.of(*asking.detailKey()) == "Trusted time ({})");
    CHECK(asking.timeServer() == std::optional<std::string>{"time.cloudflare.com"});
    CHECK(text.lines(at(1, Step::TrustedTime, Activity::SyncingClock)) == (Lines{"Checking the time", "Trusted time"}));
}

TEST(fetch_page_line_needs_a_page_read)
{
    CHECK(!at(1, Step::OwnedLibrary, Activity::ReadingEntitlements).pageKey());
    CHECK(!at(1, Step::OwnedLibrary, std::nullopt, 3).pageKey());
}

TEST(fetch_forced_refresh_follows_the_attempt_after_the_start_over)
{
    const EnglishText text;
    FollowedFetch fetch;
    fetch.follow(1);
    CHECK(fetch.observe(1, at(5, Step::OwnedLibrary, Activity::ReadingEntitlements, 2)) ==
          FollowedFetch::Change::Step);

    fetch.follow(2);
    const std::optional<uint64_t> superseded = 5;
    const std::vector<FetchProgress> watched = {
        at(5, Step::OwnedLibrary, Activity::ReadingEntitlements, 3),
        at(5, Step::Cancelled),
        at(6, Step::CheckingSignIn),
        at(6, Step::CloudList, Activity::WalkingPublicStore, 1, 2),
    };

    CHECK(fetch.observe(1, watched[1]) == FollowedFetch::Change::Dropped);
    CHECK(!fetch.latest().has_value());

    std::vector<Lines> shown;
    for (const FetchProgress& progress : watched)
        if (progress.begunAfter(superseded) && fetch.observe(2, progress) != FollowedFetch::Change::Dropped)
            shown.push_back(text.lines(*fetch.latest()));

    CHECK(shown == (std::vector<Lines>{{"Checking your sign-in"},
                                       {"Reading the cloud catalog", "PlayStation Store", "Page 1 of 2"}}));
    CHECK(fetch.latest()->attempt() == 6u);
}

TEST(fetch_attempts_count_as_begun_after_nothing_superseded)
{
    CHECK(at(1, Step::CheckingSignIn).begunAfter(std::nullopt));
    CHECK(!at(3, Step::CloudList).begunAfter(3));
    CHECK(at(4, Step::CheckingSignIn).begunAfter(3));
}
