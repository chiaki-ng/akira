#include "test_util.hpp"

#include "cloud/game_actions.hpp"
#include "cloud/launch_checks.hpp"
#include "cloud/start_steps.hpp"

#include <vector>

using namespace cloud;

namespace {

Game ps5Game()
{
    Game game;
    game.productId = "EP0001-PPSA00001_00-GAME000000000000";
    game.name = "Game";
    game.platform = "ps5";
    game.streamServiceType = "pscloud";
    game.conceptUrl = "https://store.playstation.com/concept/1";
    return game;
}

}

TEST(play_needs_a_passed_store_check_when_one_is_needed)
{
    Game game = ps5Game();
    game.isOwned = true;
    game.needsStorefrontCheck = true;

    GameActions unchecked(game, true, std::nullopt, true);
    CHECK(unchecked.play().has_value());
    CHECK(!unchecked.play()->enabled());
    CHECK(unchecked.store() == GameActions::StoreTitle::ViewStorePage);
    CHECK(unchecked.focusOrder() == (std::vector<GameActions::Focus>{GameActions::Focus::Store}));

    GameActions streams(game, true, CardCheck::streams(), true);
    CHECK(streams.play()->enabled());
    CHECK(GameActions::standingAfter(game, CardCheck::streams()) == Standing::InLibrary);

    GameActions refused(game, true, CardCheck::notStreamable(), true);
    CHECK(refused.store() == GameActions::StoreTitle::ViewInStore);
    CHECK(GameActions::standingAfter(game, CardCheck::notStreamable()) == Standing::Store);
}

TEST(library_adds_offer_refresh_whenever_no_refresh_is_running)
{
    Game game = ps5Game();
    game.needsLibraryAdd = true;
    game.canBeAddedToLibrary = true;
    game.plusCatalog = true;

    GameActions idle(game, true, std::nullopt, true);
    CHECK(!idle.play().has_value());
    CHECK(idle.addsToLibrary());
    CHECK(idle.refreshTitleFocusable());
    CHECK(!idle.showsLaunchChecks());
    CHECK(idle.focusOrder() ==
          (std::vector<GameActions::Focus>{GameActions::Focus::AddToLibrary, GameActions::Focus::RefreshTitle}));

    GameActions busy(game, true, std::nullopt, false);
    CHECK(!busy.refreshTitleFocusable());
    CHECK(busy.focusOrder() == (std::vector<GameActions::Focus>{GameActions::Focus::AddToLibrary}));
}

TEST(refused_launches_and_bundles_shape_the_play_offer)
{
    Game game = ps5Game();
    game.isOwned = true;
    game.launchBlock = LaunchBlock::Refused;
    CHECK(!GameActions(game, true, std::nullopt, true).play()->enabled());

    game.launchBlock = LaunchBlock::ChooseAGame;
    game.bundle = {BundledGame{"EP0001-PPSA00002_00-GAME000000000000", "E1", true}};
    CHECK(GameActions(game, true, std::nullopt, true).play()->offer == GameActions::Offer::ChooseGame);
    CHECK(!GameActions(game, false, std::nullopt, true).play()->enabled());
}

TEST(launch_checks_mark_steps_as_they_are_reached)
{
    Game game = ps5Game();
    LaunchChecks checks(game.productId, 1, game);
    CHECK(checks.steps().size() == 5);
    CHECK(checks.mark(LaunchChecks::Step::Premium) == LaunchChecks::Mark::Unchecked);

    checks.observe(LaunchChecks::Check::Ownership);
    CHECK(checks.mark(LaunchChecks::Step::Premium) == LaunchChecks::Mark::Passed);
    CHECK(checks.mark(LaunchChecks::Step::YourCopy) == LaunchChecks::Mark::Checking);
    checks.observe(LaunchChecks::Check::CloudStreaming);
    CHECK(checks.mark(LaunchChecks::Step::YourCopy) == LaunchChecks::Mark::Checking);

    checks.refuse("no");
    CHECK(checks.mark(LaunchChecks::Step::YourCopy) == LaunchChecks::Mark::Refused);
    CHECK(checks.mark(LaunchChecks::Step::Release) == LaunchChecks::Mark::Unchecked);
    CHECK(!checks.isRunning());
    checks.clear();
    CHECK(checks.phase() == LaunchChecks::Phase::Refused);

    Game classic = ps5Game();
    classic.streamServiceType = "psnow";
    LaunchChecks old(classic.productId, 2, classic);
    CHECK(old.steps() == (std::vector<LaunchChecks::Step>{LaunchChecks::Step::Membership, LaunchChecks::Step::Release}));
    old.clear();
    CHECK(old.mark(LaunchChecks::Step::Release) == LaunchChecks::Mark::Passed);
    old.launched();
    CHECK(old.phase() == LaunchChecks::Phase::Launched);
}

TEST(start_steps_only_move_forward_and_fail_where_they_stopped)
{
    StartSteps steps;
    CHECK(steps.mark(StartSteps::Step::Checking) == StartSteps::Mark::Running);
    CHECK(steps.advance(StartSteps::Step::FindingServer));
    CHECK(!steps.advance(StartSteps::Step::Preparing));
    CHECK(steps.mark(StartSteps::Step::Preparing) == StartSteps::Mark::Done);
    CHECK(steps.mark(StartSteps::Step::FindingServer) == StartSteps::Mark::Running);
    CHECK(steps.mark(StartSteps::Step::Connecting) == StartSteps::Mark::Pending);
    steps.fail();
    CHECK(steps.mark(StartSteps::Step::FindingServer) == StartSteps::Mark::Failed);
    CHECK(!steps.advance(StartSteps::Step::Connecting));

    StartSteps played;
    played.playing();
    CHECK(played.mark(StartSteps::Step::Connecting) == StartSteps::Mark::Done);
}
