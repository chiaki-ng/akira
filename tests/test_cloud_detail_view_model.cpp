#include "test_util.hpp"

#include "cloud/game_actions.hpp"
#include "cloud/game_facts.hpp"
#include "cloud/launch_checks.hpp"
#include "cloud/release_notice.hpp"
#include "cloud/title_refresh_cooldown.hpp"

#include <chrono>
#include <string>
#include <vector>

using namespace akira::cloud;

namespace {

Game game(const std::string& id, const std::string& name)
{
    Game entry;
    entry.productId = id;
    entry.name = name;
    entry.platform = "ps5";
    entry.streamServiceType = "pscloud";
    return entry;
}

}

TEST(launch_checks_map_each_core_check)
{
    using Core = libchiaking::catalog::LaunchCheck;
    CHECK(LaunchChecks::checkOf(Core::loading_library) == LaunchChecks::Check::LoadingLibrary);
    CHECK(LaunchChecks::checkOf(Core::membership) == LaunchChecks::Check::Membership);
    CHECK(LaunchChecks::checkOf(Core::cloud_streaming) == LaunchChecks::Check::CloudStreaming);
    CHECK(LaunchChecks::checkOf(Core::ownership) == LaunchChecks::Check::Ownership);
    CHECK(LaunchChecks::checkOf(Core::entitlement) == LaunchChecks::Check::Entitlement);
    CHECK(LaunchChecks::checkOf(Core::cloud_game) == LaunchChecks::Check::CloudGame);
    CHECK(LaunchChecks::checkOf(Core::exclusions) == LaunchChecks::Check::Exclusions);
    CHECK(LaunchChecks::checkOf(Core::release) == LaunchChecks::Check::Release);
}

TEST(standing_note_names_demos_and_trials)
{
    Game trial = game("T", "Big Game Demo");
    trial.isTrial = true;
    CHECK(GameActions::standingNote(Standing::MayStream, trial) == GameActions::StandingNote::Demo);
    trial.name = "Demolition Derby";
    CHECK(GameActions::standingNote(Standing::InLibrary, trial) == GameActions::StandingNote::Trial);
    CHECK(GameActions::standingNote(Standing::Store, trial) == GameActions::StandingNote::Store);
    CHECK(GameActions::standingNote(Standing::OwnershipUnknown, trial) == GameActions::StandingNote::Store);

    const Game full = game("F", "Demo");
    CHECK(GameActions::standingNote(Standing::Included, full) == GameActions::StandingNote::Included);
    CHECK(GameActions::standingNote(Standing::MayStream, full) == GameActions::StandingNote::MayStream);
}

TEST(bundle_choices_borrow_listed_games_and_skip_unlaunchable)
{
    Game bundle = game("BUNDLE", "Bundle");
    bundle.launchBlock = LaunchBlock::ChooseAGame;
    bundle.bundle = {{"CHILD1", "", true}, {"CHILD2", "ENT2", true}, {"CHILD3", "", false}, {"", "", true}};
    Game listedChild = game("OTHER", "Child Two");
    listedChild.entitlementId = "ENT2";

    const auto choices = GameActions::bundleChoices(bundle, {bundle, listedChild});
    REQUIRE(choices.size() == 2u);
    CHECK_EQ(choices[0].productId, std::string("CHILD1"));
    CHECK_EQ(choices[0].name, std::string("Bundle"));
    CHECK(choices[0].bundle.empty());
    CHECK(choices[0].launchBlock == LaunchBlock::None);
    CHECK_EQ(choices[1].productId, std::string("CHILD2"));
    CHECK_EQ(choices[1].name, std::string("Child Two"));
}

TEST(refocus_prefers_newly_enabled_play_then_the_previous_button)
{
    Game owned = game("P", "Play");
    owned.isOwned = true;
    const GameActions actions(owned, true, std::nullopt, true);
    const auto all = [](GameActions::Focus) { return true; };
    CHECK(actions.refocus(std::nullopt, false, all) == GameActions::Focus::Play);
    CHECK(actions.refocus(GameActions::Focus::Store, true, all) == GameActions::Focus::Store);

    const auto noAdd = [](GameActions::Focus focus) { return focus != GameActions::Focus::AddToLibrary; };
    CHECK(actions.refocus(GameActions::Focus::AddToLibrary, true, noAdd) == GameActions::Focus::RefreshTitle);

    const auto none = [](GameActions::Focus) { return false; };
    CHECK(!actions.refocus(GameActions::Focus::Play, false, none).has_value());
}

TEST(release_notice_counts_down_a_week_then_shows_the_date)
{
    Game later = game("L", "Later");
    CHECK(!ReleaseNotice::of(later, 0).has_value());

    later.release = ReleaseKind::Unannounced;
    REQUIRE(ReleaseNotice::of(later, 0).has_value());
    CHECK(ReleaseNotice::of(later, 0)->kind() == ReleaseNotice::Kind::Unannounced);

    later.release = ReleaseKind::Scheduled;
    later.availableFromMs = 10'500;
    REQUIRE(ReleaseNotice::of(later, 0).has_value());
    CHECK(ReleaseNotice::of(later, 0)->kind() == ReleaseNotice::Kind::Countdown);
    CHECK_EQ(ReleaseNotice::of(later, 0)->seconds(), int64_t(11));
    CHECK(!ReleaseNotice::of(later, 10'500).has_value());

    later.availableFromMs = 8LL * 86400 * 1000;
    REQUIRE(ReleaseNotice::of(later, 0).has_value());
    CHECK(ReleaseNotice::of(later, 0)->kind() == ReleaseNotice::Kind::Date);
    CHECK_EQ(ReleaseNotice::of(later, 0)->atMs(), later.availableFromMs);
}

TEST(game_facts_list_publisher_release_and_genres)
{
    GameDetail detail;
    CHECK(GameFacts(detail).entries().empty());
    detail.publisher = "Studio";
    detail.releasedMs = 0;
    detail.genres = {"Action", "Puzzle"};
    const GameFacts facts(detail);
    REQUIRE(facts.entries().size() == 3u);
    CHECK(facts.entries()[0].fact == GameFacts::Fact::Publisher);
    CHECK_EQ(facts.entries()[0].value, std::string("Studio"));
    CHECK(facts.entries()[1].fact == GameFacts::Fact::Released);
    CHECK_EQ(facts.entries()[1].value, std::string("01 January 1970"));
    CHECK(facts.entries()[2].fact == GameFacts::Fact::Genres);
    CHECK_EQ(facts.entries()[2].value, std::string("Action, Puzzle"));
}

TEST(title_refresh_cooldown_runs_one_check_then_waits)
{
    using namespace std::chrono_literals;
    const auto start = TitleRefreshCooldown::Clock::time_point{} + 100s;
    TitleRefreshCooldown cooldown(15s);
    CHECK(cooldown.idle());

    const auto attempt = cooldown.begin(start);
    REQUIRE(attempt.has_value());
    CHECK(cooldown.phase() == TitleRefreshCooldown::Phase::Checking);
    CHECK(!cooldown.begin(start).has_value());
    CHECK_EQ(cooldown.secondsLeft(start), 15);

    TitleRefresh refreshed;
    refreshed.outcome = TitleRefresh::Outcome::Failed;
    refreshed.message = "nope";
    CHECK(!cooldown.finish(*attempt + 1, refreshed, start + 1s));
    CHECK(cooldown.finish(*attempt, refreshed, start + 1s));
    CHECK(cooldown.phase() == TitleRefreshCooldown::Phase::CoolingDown);
    CHECK(cooldown.outcome() == TitleRefresh::Outcome::Failed);
    CHECK_EQ(cooldown.message(), std::string("nope"));
    CHECK(!cooldown.finish(*attempt, refreshed, start + 2s));

    CHECK(cooldown.tick(start + 2s) == TitleRefreshCooldown::Tick::Second);
    CHECK(cooldown.tick(start + 2s) == TitleRefreshCooldown::Tick::Unchanged);
    CHECK(cooldown.tick(start + 15s) == TitleRefreshCooldown::Tick::Idle);
    CHECK(cooldown.idle());
    CHECK(cooldown.tick(start + 16s) == TitleRefreshCooldown::Tick::Unchanged);

    const auto again = cooldown.begin(start + 20s);
    REQUIRE(again.has_value());
    CHECK(*again != *attempt);
    CHECK(!cooldown.outcome().has_value());
    refreshed.outcome = TitleRefresh::Outcome::Added;
    CHECK(cooldown.finish(*again, refreshed, start + 40s));
    CHECK(cooldown.idle());
}
