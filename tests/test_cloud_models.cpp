#include "test_util.hpp"

#include "cloud/models.hpp"

#include <cstdint>
#include <limits>

using namespace cloud;

TEST(catalog_finds_a_game_by_product_and_service)
{
    Catalog catalog;
    Game owned;
    owned.productId = "PPSA1";
    owned.streamServiceType = "pscloud";
    owned.isOwned = true;
    Game addable;
    addable.productId = "PPSA2";
    addable.streamServiceType = "pscloud";
    addable.needsLibraryAdd = true;
    Game classic;
    classic.productId = "CUSA3";
    classic.streamServiceType = "psnow";
    catalog.games = {owned, addable, classic};

    Game pinned;
    pinned.productId = "PPSA2";
    pinned.streamServiceType = "pscloud";
    const Game* current = catalog.find(pinned);
    CHECK(current != nullptr);
    CHECK(current->requiresLibraryAdd());

    pinned.productId = "PPSA1";
    CHECK(!catalog.find(pinned)->requiresLibraryAdd());

    pinned.productId = "CUSA3";
    CHECK(catalog.find(pinned) == nullptr);
    pinned.streamServiceType = "psnow";
    CHECK(catalog.find(pinned) != nullptr);

    CHECK_EQ(catalog.launchableCount(), 3);
}

TEST(games_without_a_product_are_not_launchable)
{
    Game game;
    game.streamServiceType = "pscloud";
    CHECK(!game.launchable());
    game.productId = "PPSA1";
    CHECK(game.launchable());
    game.category = "purchaseable";
    CHECK(!game.streamableNow());
    game.isOwned = true;
    CHECK(game.streamableNow());
}

TEST(release_kinds_decide_what_awaits_release)
{
    Game game;
    CHECK(!game.awaitsRelease(0));
    game.release = ReleaseKind::Scheduled;
    game.availableFromMs = 1000;
    CHECK(game.awaitsRelease(999));
    CHECK(!game.awaitsRelease(1000));
    CHECK(game.unlockAfter(0) == std::optional<int64_t>(1000));
    CHECK(!game.unlockAfter(1000));
    game.release = ReleaseKind::Unannounced;
    CHECK(game.awaitsRelease(std::numeric_limits<int64_t>::max()));
    CHECK(!game.unlockAfter(0));
}

TEST(countdown_reads_days_then_a_clock)
{
    CHECK_EQ(Game::countdown(59), std::string("00:00:59"));
    CHECK_EQ(Game::countdown(3 * 86400 + 3661), std::string("3d 01:01:01"));
    CHECK_EQ(Game::countdown(-5), std::string("00:00:00"));
}

TEST(artwork_prefers_the_landscape_cover)
{
    Game game;
    game.imageUrl = "https://x/a.png";
    CHECK_EQ(game.artworkUrl(), std::string("https://x/a.png"));
    game.landscapeImageUrl = "https://x/l.png";
    CHECK_EQ(game.artworkUrl(), std::string("https://x/l.png"));
}

TEST(psn_plus_membership_is_only_a_definitive_negative_gate)
{
    CHECK(observePlusMembership(false) == PlusMembership::None);
    CHECK(observePlusMembership(true) == PlusMembership::SomeTier);
    CHECK(observePlusMembership(std::nullopt) == PlusMembership::Unknown);
}
