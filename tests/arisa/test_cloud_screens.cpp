#include "test_util.hpp"

#include <chrono>
#include <string>
#include <vector>

#include "cloud/launch_report.hpp"
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

    auto openFirstCard() -> bool
    {
        if (!shots.pad(arisa::provider::Buttons{ arisa::provider::Button::dpad_right }))
            return false;
        for (int step = 0; step < 6 && !shots.focusedWithin("cloud.grid"); ++step)
            if (!shots.pad(arisa::provider::Buttons{ arisa::provider::Button::dpad_down }))
                return false;
        return shots.focusedWithin("cloud.grid") && shots.pad(arisa::provider::Buttons{ arisa::provider::Button::a })
            && shots.focusedWithin("cloud.game");
    }

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

TEST(cloud_model_retries_a_dropped_session_from_its_game_page)
{
    tests::FakeCloudDesk desk;
    tests::CollectedNotices notices;
    tests::FakeStrings strings({});
    CloudModel model(desk, notices, strings);
    model.appear();
    akira::cloud::Game previous;
    previous.productId = tests::FakeCloudDesk::catalog()[3].productId;
    previous.name = "Gran Turismo 7";
    previous.serviceType = "pscloud";
    model.retry(previous);
    REQUIRE(model.view().get().detail.has_value());
    CHECK(model.view().get().detail->game.imageUrl == tests::FakeCloudDesk::catalog()[3].imageUrl);
    CHECK(desk.clearing == previous.productId);
    CHECK(desk.launches == 1);
    model.retry(previous);
    CHECK(desk.launches == 2);
    CHECK(desk.cancels == 1);
}

TEST(cloud_model_retries_a_game_missing_from_the_catalog)
{
    tests::FakeCloudDesk desk;
    tests::CollectedNotices notices;
    tests::FakeStrings strings({});
    CloudModel model(desk, notices, strings);
    model.appear();
    akira::cloud::Game previous;
    previous.productId = "EP0000-PPSA00000_00-NOTLISTED0000000";
    previous.name = "Unlisted";
    previous.serviceType = "pscloud";
    model.retry(previous);
    REQUIRE(model.view().get().detail.has_value());
    CHECK(model.view().get().detail->game.name == "Unlisted");
    CHECK(desk.launches == 1);
}

TEST(arisa_cloud_game_page_keeps_focus_on_play_through_the_checks)
{
    CloudShots cloud;
    cloud.desk.holdsLaunch = true;
    cloud.desk.observed = { LaunchChecks::Check::Membership };
    REQUIRE(cloud.start());
    REQUIRE(cloud.openFirstCard());
    CHECK(cloud.shots.focusedWithin("cloud.game.play"));
    REQUIRE(cloud.shots.press("cloud.game.play"));
    CHECK(cloud.shots.focusedWithin("cloud.game"));
    CHECK(!cloud.shots.focusedWithin("cloud.game.play"));
    REQUIRE(static_cast<bool>(cloud.desk.heldRefuse));
    cloud.desk.heldRefuse("Cloud streaming is not available right now.");
    REQUIRE(cloud.shots.settle());
    CHECK(cloud.shots.focusedWithin("cloud.game.play"));
}

TEST(arisa_cloud_game_page_moves_focus_to_play_once_the_card_check_allows_it)
{
    CloudShots cloud;
    cloud.desk.holdsCardCheck = true;
    for (akira::cloud::Game& game : cloud.desk.listed.catalog.games)
        game.needsStorefrontCheck = true;
    REQUIRE(cloud.start());
    REQUIRE(cloud.openFirstCard());
    CHECK(cloud.shots.focusedWithin("cloud.game"));
    CHECK(!cloud.shots.focusedWithin("cloud.game.play"));
    REQUIRE(static_cast<bool>(cloud.desk.heldCardCheck));
    cloud.desk.heldCardCheck(akira::cloud::CardCheck::streams());
    REQUIRE(cloud.shots.settle());
    CHECK(cloud.shots.focusedWithin("cloud.game.play"));
}

TEST(arisa_cloud_game_page_leaves_a_chosen_button_alone)
{
    CloudShots cloud;
    cloud.desk.holdsCardCheck = true;
    for (akira::cloud::Game& game : cloud.desk.listed.catalog.games)
        game.needsStorefrontCheck = true;
    REQUIRE(cloud.start());
    REQUIRE(cloud.openFirstCard());
    REQUIRE(static_cast<bool>(cloud.desk.heldCardCheck));
    cloud.desk.heldCardCheck(akira::cloud::CardCheck::streams());
    REQUIRE(cloud.shots.settle());
    REQUIRE(cloud.shots.pad(arisa::provider::Buttons{ arisa::provider::Button::dpad_right }));
    CHECK(cloud.shots.focusedWithin("cloud.game.store"));
    cloud.desk.holdsLaunch = true;
    cloud.model.play();
    REQUIRE(cloud.shots.settle());
    REQUIRE(static_cast<bool>(cloud.desk.heldRefuse));
    cloud.desk.heldRefuse("Cloud streaming is not available right now.");
    REQUIRE(cloud.shots.settle());
    CHECK(cloud.shots.focusedWithin("cloud.game.store"));
}

TEST(arisa_shot_cloud_launch_progress)
{
    using libchiaking::play::LaunchActivity;
    using libchiaking::play::LaunchProgress;
    const auto progress = LaunchProgress::at(libchiaking::catalog::ServiceGeneration::ps5_cloud,
        LaunchActivity::allocating_server,
        LaunchProgress::Counts{ .step = LaunchProgress::Step{ .number = 5, .of = 7 },
            .queue_position = 4,
            .wait_time_estimate_seconds = 150,
            .attempt = 2,
            .wait = libchiaking::progress::Wait{ .reason = libchiaking::progress::WaitReason::server_queue,
                .next_try_in = std::chrono::milliseconds(10'000) },
            .elapsed = std::chrono::seconds(42) });
    REQUIRE(progress.has_value());
    CloudShots cloud;
    akira::cloud::LaunchReport report;
    const auto now = akira::cloud::LaunchReport::Clock::now();
    report.follow(*progress, now);
    const auto& strings = cloud.shots.text();
    CHECK(report.step() == 3);
    CHECK(report.status(strings) == strings.format("akira/cloud/start_queued", 4));
    CHECK(report.activity(strings) == strings.text("akira/cloud/activity_allocating"));
    cloud.shots.model.connecting("Astro Bot");
    cloud.shots.model.stage(akira::play::ui::ConnectingScreen::Progress{ .console = "Astro Bot",
        .status = report.status(strings),
        .step = report.step(),
        .steps = akira::cloud::LaunchReport::steps,
        .activity = report.activity(strings),
        .details = report.details(strings, now + std::chrono::seconds(3)) });
    REQUIRE(cloud.start());
    CHECK(cloud.shots.has("connecting.activity"));
    CHECK(cloud.shots.has("connecting.details"));
    CHECK(cloud.shots.write("cloud_launch_progress"));
}

TEST(arisa_shot_cloud_stopped_offers_retry)
{
    CloudShots cloud;
    const auto& strings = cloud.shots.text();
    const std::string text = strings.format("akira/cloud/start_stopped", strings.text("akira/connection/fail_generic"));
    const std::size_t split = text.find("\n\n");
    cloud.shots.model.ended(akira::app::arisa::ShellModel::Ending{ .title = text.substr(0, split),
        .message = text.substr(split + 2),
        .reconnect = true,
        .retry = true });
    REQUIRE(cloud.start());
    CHECK(cloud.shots.focusedWithin("ending.yes"));
    CHECK(cloud.shots.write("alert_cloud_stopped"));
    REQUIRE(cloud.shots.press("ending.no"));
    CHECK(!cloud.shots.model.view().get().ending.has_value());
}
