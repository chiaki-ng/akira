#include "test_util.hpp"

#include <string>
#include <vector>

#include "cloud/library_listing.hpp"
#include "cloud/ui/cloud_model.hpp"
#include "fake_cloud_desk.hpp"
#include "fake_strings.hpp"
#include "screen_shots.hpp"

namespace {

using akira::cloud::LaunchChecks;
using akira::cloud::LibraryFilter;
using akira::cloud::PlatformFilter;
using akira::cloud::ui::CloudModel;

class CloudShots {
public:
    CloudShots()
        : model(desk, shots.collected, shots.text())
    {
        parts = shots.parts();
        parts.cloud = &model;
        shots.model.offer(true, false);
        shots.model.show(akira::app::arisa::ShellModel::Page::Cloud);
    }

    auto start() -> bool { return shots.start(shots.shell(parts)); }

    tests::FakeCloudDesk desk;
    tests::ScreenShots shots;
    CloudModel model;
    akira::app::arisa::ShellScreen::Parts parts{};
};

}

TEST(cloud_model_loads_the_catalog_and_keeps_recent_first)
{
    tests::FakeCloudDesk desk;
    tests::CollectedNotices notices;
    tests::FakeStrings strings({});
    CloudModel model(desk, notices, strings);
    model.appear();
    const CloudModel::View& view = model.view().get();
    CHECK(desk.refreshes == 1);
    CHECK(view.hasCatalog);
    CHECK(view.listed == 10);
    CHECK(view.filter == LibraryFilter::Recent);
    REQUIRE(!view.games.empty());
    CHECK(view.games.front().name == "Astro Bot");
    model.appear();
    CHECK(desk.refreshes == 1);
}

TEST(cloud_model_filters_by_platform_and_query)
{
    tests::FakeCloudDesk desk;
    tests::CollectedNotices notices;
    tests::FakeStrings strings({});
    CloudModel model(desk, notices, strings);
    model.appear();
    model.pickFilter(LibraryFilter::All);
    CHECK(desk.savedFilter == "all");
    model.pickPlatform(PlatformFilter::PS4);
    CHECK(model.view().get().games.size() == 1);
    model.pickPlatform(PlatformFilter::All);
    model.search("ra");
    for (const akira::cloud::Game& game : model.view().get().games)
        CHECK(game.name.find("ra") != std::string::npos || game.name.find("Ra") != std::string::npos);
    model.sortBy(1);
    CHECK(desk.savedSort == 1);
}

TEST(cloud_model_pins_and_unpins_shortcuts)
{
    tests::FakeCloudDesk desk;
    tests::CollectedNotices notices;
    tests::FakeStrings strings({});
    CloudModel model(desk, notices, strings);
    model.appear();
    const akira::cloud::Game first = model.view().get().games.front();
    model.toggleShortcut(first);
    CHECK(desk.pinned.size() == 1);
    CHECK(model.pinned(first));
    model.toggleShortcut(first);
    CHECK(desk.pinned.empty());
    CHECK(notices.shown.size() == 2);
}

TEST(cloud_model_shows_the_failure_snapshot)
{
    tests::FakeCloudDesk desk;
    desk.answers = false;
    desk.progress = akira::cloud::FetchProgress(1, akira::cloud::FetchProgress::Step::Unanswered, std::nullopt, 0,
        std::nullopt, {}, akira::cloud::FetchFailure(akira::cloud::FetchFailure::Reason::User, 401, true));
    desk.listed = akira::cloud::Snapshot{};
    tests::CollectedNotices notices;
    tests::FakeStrings strings({});
    CloudModel model(desk, notices, strings);
    model.appear();
    CHECK(!model.view().get().hasCatalog);
    CHECK(model.view().get().status.availability == akira::cloud::Availability::NeedsPairing);
}

TEST(cloud_model_opens_a_game_and_launches_after_the_checks)
{
    tests::FakeCloudDesk desk;
    desk.observed = { LaunchChecks::Check::Membership, LaunchChecks::Check::CloudStreaming };
    tests::CollectedNotices notices;
    tests::FakeStrings strings({});
    CloudModel model(desk, notices, strings);
    model.appear();
    model.open(model.view().get().games.front());
    REQUIRE(model.view().get().detail.has_value());
    CHECK(model.view().get().detail->facts.has_value());
    model.play();
    CHECK(desk.launches == 1);
    REQUIRE(model.view().get().detail->checks.has_value());
    CHECK(model.view().get().detail->checks->phase() == LaunchChecks::Phase::Launched);
    model.close();
    CHECK(!model.view().get().detail.has_value());
}

TEST(cloud_model_keeps_the_refusal_reason)
{
    tests::FakeCloudDesk desk;
    desk.refusal = std::string("Cloud streaming needs a PlayStation Plus tier that includes game streaming.");
    tests::CollectedNotices notices;
    tests::FakeStrings strings({});
    CloudModel model(desk, notices, strings);
    model.appear();
    model.open(model.view().get().games.front());
    model.play();
    CHECK(desk.launches == 0);
    REQUIRE(model.view().get().detail->checks.has_value());
    CHECK(model.view().get().detail->checks->reason().has_value());
}

TEST(cloud_model_shows_the_store_code)
{
    tests::FakeCloudDesk desk;
    tests::CollectedNotices notices;
    tests::FakeStrings strings({});
    CloudModel model(desk, notices, strings);
    model.appear();
    model.open(model.view().get().games.front());
    model.showStore(false);
    REQUIRE(model.view().get().code.has_value());
    CHECK(model.view().get().code->url == "https://store.playstation.com/concept/10001234");
    model.dismissCode();
    CHECK(!model.view().get().code.has_value());
}

TEST(arisa_shot_cloud_library)
{
    CloudShots cloud;
    REQUIRE(cloud.start());
    cloud.model.pickFilter(LibraryFilter::All);
    REQUIRE(cloud.shots.settle() && cloud.shots.settleArt());
    CHECK(cloud.shots.has("cloud.grid"));
    CHECK(cloud.shots.fixtures().requests() > 0);
    CHECK(cloud.shots.write("cloud_library"));
}

TEST(arisa_shot_cloud_library_filtered)
{
    CloudShots cloud;
    cloud.desk.pinned = { tests::FakeCloudDesk::catalog()[1], tests::FakeCloudDesk::catalog()[5] };
    REQUIRE(cloud.start());
    cloud.model.pickFilter(LibraryFilter::All);
    cloud.model.pickPlatform(PlatformFilter::PS5);
    cloud.model.search("o");
    REQUIRE(cloud.shots.settle() && cloud.shots.settleArt());
    CHECK(cloud.shots.has("cloud.shortcuts"));
    CHECK(cloud.shots.write("cloud_library_filtered"));
}

TEST(arisa_shot_cloud_game)
{
    CloudShots cloud;
    REQUIRE(cloud.start());
    cloud.model.open(tests::FakeCloudDesk::catalog()[0]);
    REQUIRE(cloud.shots.settle() && cloud.shots.settleArt());
    CHECK(cloud.shots.has("cloud.game.band"));
    CHECK(cloud.shots.write("cloud_game"));
}

TEST(arisa_shot_cloud_launch_checks)
{
    CloudShots cloud;
    cloud.desk.clears = false;
    cloud.desk.observed = { LaunchChecks::Check::LoadingLibrary, LaunchChecks::Check::Membership,
        LaunchChecks::Check::CloudStreaming };
    REQUIRE(cloud.start());
    cloud.model.open(tests::FakeCloudDesk::catalog()[0]);
    cloud.model.play();
    REQUIRE(cloud.shots.settle() && cloud.shots.settleArt());
    CHECK(cloud.shots.has("cloud.game.checks"));
    CHECK(cloud.shots.write("cloud_launch_checks"));
}

TEST(arisa_shot_cloud_state_pair)
{
    CloudShots cloud;
    cloud.desk.listed = tests::FakeCloudDesk::unpaired();
    REQUIRE(cloud.start());
    CHECK(cloud.shots.has("cloud.state.pair"));
    CHECK(cloud.shots.write("cloud_state_pair"));
}
