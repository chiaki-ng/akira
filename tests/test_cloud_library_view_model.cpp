#include "test_util.hpp"

#include "cloud/catalog_rows.hpp"
#include "cloud/cloud_shortcuts.hpp"
#include "cloud/game_badges.hpp"
#include "cloud/library_listing.hpp"
#include "cloud/library_status.hpp"

#include <string>
#include <vector>

using namespace akira::cloud;

namespace {

Game named(const std::string& id, const std::string& name, bool owned)
{
    Game game;
    game.productId = id;
    game.name = name;
    game.platform = "ps5";
    game.isOwned = owned;
    game.streamServiceType = "pscloud";
    game.serviceType = "pscloud";
    game.category = owned ? "owned" : "purchaseable";
    game.plusCatalog = !owned;
    return game;
}

std::vector<std::string> names(const std::vector<Game>& games)
{
    std::vector<std::string> out;
    for (const Game& game : games)
        out.push_back(game.name);
    return out;
}

Status status(Availability availability, bool canPair = false)
{
    Status st;
    st.availability = availability;
    st.canPair = canPair;
    return st;
}

}

TEST(library_order_reads_the_saved_sort_state)
{
    CHECK(LibraryListing::orderOf(0) == LibraryOrder::Playable);
    CHECK(LibraryListing::orderOf(1) == LibraryOrder::AToZ);
    CHECK(LibraryListing::orderOf(2) == LibraryOrder::ZToA);
    CHECK(LibraryListing::orderOf(7) == LibraryOrder::Playable);
}

TEST(library_ordered_sorts_by_name_ignoring_case_or_playable_first)
{
    LibraryListing listing;
    listing.setGames({named("A", "beta", false), named("B", "Alpha", true), named("C", "gamma", true)});
    listing.pick(LibraryFilter::All);
    CHECK(names(listing.ordered(LibraryOrder::AToZ)) == (std::vector<std::string>{"Alpha", "beta", "gamma"}));
    CHECK(names(listing.ordered(LibraryOrder::ZToA)) == (std::vector<std::string>{"gamma", "beta", "Alpha"}));
    CHECK(names(listing.ordered(LibraryOrder::Playable)) == (std::vector<std::string>{"Alpha", "gamma", "beta"}));
}

TEST(library_ordered_keeps_recency_on_the_recent_tab)
{
    LibraryListing listing;
    listing.setGames({named("A", "beta", true), named("B", "Alpha", true)});
    listing.setRecentlyPlayed({{"A", 2}, {"B", 1}});
    listing.pick(LibraryFilter::Recent);
    CHECK(names(listing.ordered(LibraryOrder::AToZ)) == (std::vector<std::string>{"beta", "Alpha"}));
    listing.setQuery("a");
    CHECK(names(listing.ordered(LibraryOrder::AToZ)) == (std::vector<std::string>{"Alpha", "beta"}));
}

TEST(library_unlock_delay_adds_a_margin_and_caps_at_a_day)
{
    LibraryListing listing;
    CHECK(!listing.unlockDelay().has_value());
    Game later = named("L", "Later", true);
    later.release = ReleaseKind::Scheduled;
    later.availableFromMs = 11000;
    listing.setNow(1000);
    listing.setGames({later});
    CHECK(listing.unlockDelay() == std::optional<int64_t>(12000));
    later.availableFromMs = 1000 + 48LL * 3600 * 1000;
    listing.setGames({later});
    CHECK(listing.unlockDelay() == std::optional<int64_t>(24LL * 3600 * 1000));
}

TEST(library_status_chips_glyphs_and_actions)
{
    CHECK(!LibraryStatus(status(Availability::NoProfile)).chip().has_value());
    CHECK(LibraryStatus(status(Availability::Ready)).chip() == LibraryStatus::Chip::Ready);
    CHECK(LibraryStatus(status(Availability::LaunchBlocked)).chip() == LibraryStatus::Chip::Warning);

    const LibraryStatus pairing(status(Availability::NeedsPairing, true));
    CHECK(pairing.glyph() == LibraryStatus::Glyph::Pair);
    CHECK(pairing.tone() == LibraryStatus::Tone::Accent);
    CHECK(pairing.action() == LibraryStatus::Action::Pair);

    const LibraryStatus empty(status(Availability::Empty));
    CHECK(empty.emptyLibrary());
    CHECK(empty.tone() == LibraryStatus::Tone::Dim);
    CHECK(empty.action() == LibraryStatus::Action::Refresh);

    const LibraryStatus failed(status(Availability::Error));
    CHECK(failed.glyph() == LibraryStatus::Glyph::Alert);
    CHECK(failed.tone() == LibraryStatus::Tone::Danger);

    const LibraryStatus checking(status(Availability::Checking));
    CHECK(checking.checking());
    CHECK(checking.tone() == LibraryStatus::Tone::Accent);
    CHECK(checking.action() == LibraryStatus::Action::None);
}

TEST(library_status_offers_pairing_only_when_possible)
{
    using Option = LibraryStatus::Option;
    CHECK(LibraryStatus(status(Availability::Ready)).options() == (std::vector<Option>{Option::Refresh, Option::ClearCache}));
    CHECK(LibraryStatus(status(Availability::Ready, true)).options()
          == (std::vector<Option>{Option::Refresh, Option::Pair, Option::ClearCache}));
}

TEST(catalog_rows_page_games_into_rows)
{
    const CatalogRows rows(9, 4);
    CHECK_EQ(rows.rows(), std::size_t(3));
    CHECK(rows.gameAt(0, 0) == std::optional<std::size_t>(0));
    CHECK(rows.gameAt(1, 3) == std::optional<std::size_t>(7));
    CHECK(rows.gameAt(2, 0) == std::optional<std::size_t>(8));
    CHECK(!rows.gameAt(2, 1).has_value());
    CHECK(!rows.gameAt(0, 4).has_value());
    CHECK_EQ(CatalogRows(0, 4).rows(), std::size_t(0));
}

TEST(cloud_shortcuts_toggle_pins_and_refusals)
{
    CloudShortcuts shortcuts({});
    Game owned = named("A", "Owned", true);
    CHECK(shortcuts.toggle(owned) == CloudShortcuts::Toggle::Added);
    CHECK_EQ(shortcuts.games().size(), std::size_t(1));
    CHECK(shortcuts.toggle(owned) == CloudShortcuts::Toggle::Removed);
    CHECK(shortcuts.games().empty());

    Game store = named("B", "Store", false);
    CHECK(shortcuts.toggle(store) == CloudShortcuts::Toggle::NeedsLibrary);
    Game leftOut = named("C", "Left out", true);
    leftOut.launchLeftOut = true;
    CHECK(shortcuts.toggle(leftOut) == CloudShortcuts::Toggle::NotBuilt);
    Game unlaunchable = named("D", "No stream", true);
    unlaunchable.streamServiceType.clear();
    CHECK(shortcuts.toggle(unlaunchable) == CloudShortcuts::Toggle::Ignored);
    CHECK(shortcuts.games().empty());
}

TEST(game_badges_follow_platform_plus_streamability_and_category)
{
    Game game = named("A", "Owned", true);
    game.plusCatalog = true;
    game.streamabilityStatus = StreamabilityStatus::Streamable;
    const GameBadges badges(game);
    REQUIRE(badges.badges().size() == 4u);
    CHECK(badges.badges()[0].kind == GameBadges::Kind::PlatformPs5);
    CHECK_EQ(badges.badges()[0].label, std::string("PS5"));
    CHECK(badges.badges()[1].kind == GameBadges::Kind::Plus);
    CHECK(badges.badges()[2].kind == GameBadges::Kind::Verified);
    CHECK(badges.badges()[3].kind == GameBadges::Kind::CategoryOwned);
    CHECK_EQ(badges.badges()[3].label, std::string("Owned"));
    CHECK(badges.playable());

    Game classic = named("B", "Classic", false);
    classic.platform = "ps3";
    classic.serviceType = "psnow";
    classic.category = "streamable";
    classic.launchLeftOut = true;
    classic.plusCatalog = false;
    const GameBadges old(classic);
    REQUIRE(old.badges().size() == 2u);
    CHECK(old.badges()[0].kind == GameBadges::Kind::PlatformOther);
    CHECK(old.badges()[1].kind == GameBadges::Kind::CategoryCatalog);
    CHECK(!old.playable());
    CHECK(old.leftOut());
}
