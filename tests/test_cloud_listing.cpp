#include "test_util.hpp"

#include "cloud/library_listing.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace cloud;

namespace {

Game titled(const std::string& id, const std::string& platform, bool owned, bool plus, const std::string& service)
{
    Game game;
    game.productId = id;
    game.name = id;
    game.platform = platform;
    game.isOwned = owned;
    game.plusCatalog = plus;
    game.streamServiceType = service;
    game.serviceType = service;
    return game;
}

std::vector<std::string> ids(const std::vector<Game>& games)
{
    std::vector<std::string> out;
    for (const Game& game : games)
        out.push_back(game.productId);
    return out;
}

LibraryListing library()
{
    Game owned = titled("OWNED5", "ps5", true, false, "pscloud");
    Game plus = titled("PLUS5", "ps5", false, true, "pscloud");
    Game store = titled("STORE5", "ps5", false, false, "pscloud");
    Game classic = titled("CLASSIC4", "ps4", false, false, "psnow");
    Game old = titled("OLD3", "ps3", false, false, "psnow");
    Game later = titled("LATER5", "ps5", true, false, "pscloud");
    later.release = ReleaseKind::Scheduled;
    later.availableFromMs = 5000;
    Game ended = titled("ENDED5", "ps5", true, false, "pscloud");
    ended.trialEnded = true;
    Game refused = titled("REFUSED5", "ps5", true, false, "pscloud");
    refused.refused = true;

    LibraryListing listing;
    listing.setNow(1000);
    listing.setGames({owned, plus, store, classic, old, later, ended, refused});
    return listing;
}

}

TEST(tabs_admit_what_swift_admits)
{
    LibraryListing listing = library();
    CHECK(ids(listing.matching()) == (std::vector<std::string>{"OWNED5", "PLUS5", "CLASSIC4", "OLD3"}));
    listing.pick(LibraryFilter::Owned);
    CHECK(ids(listing.matching()) == (std::vector<std::string>{"OWNED5", "REFUSED5"}));
    listing.pick(LibraryFilter::Plus);
    CHECK(ids(listing.matching()) == (std::vector<std::string>{"PLUS5", "CLASSIC4", "OLD3"}));
    listing.pick(LibraryFilter::Browse);
    CHECK(ids(listing.matching()) == (std::vector<std::string>{"STORE5"}));
    listing.pick(LibraryFilter::Preorders);
    CHECK(ids(listing.matching()) == (std::vector<std::string>{"LATER5"}));
}

TEST(optional_tabs_appear_only_with_something_to_show)
{
    LibraryListing listing = library();
    auto filters = listing.filters();
    CHECK(std::ranges::find(filters, LibraryFilter::Recent) == filters.end());
    CHECK(std::ranges::find(filters, LibraryFilter::Preorders) != filters.end());

    listing.setNow(6000);
    filters = listing.filters();
    CHECK(std::ranges::find(filters, LibraryFilter::Preorders) == filters.end());

    listing.setRecentlyPlayed({{"STORE5", 1}});
    CHECK(!listing.hasRecentlyPlayed());
    listing.setRecentlyPlayed({{"OLD3", 1}});
    CHECK(listing.hasRecentlyPlayed());
}

TEST(automatic_choice_moves_to_recently_played_but_a_pick_stays)
{
    LibraryListing listing = library();
    listing.setRecentlyPlayed({{"CLASSIC4", 2}, {"OWNED5", 1}});
    listing.settle(true);
    CHECK(listing.filter() == LibraryFilter::Recent);
    CHECK(ids(listing.matching()) == (std::vector<std::string>{"CLASSIC4", "OWNED5"}));

    LibraryListing picked = library();
    picked.pick(LibraryFilter::Plus);
    picked.setRecentlyPlayed({{"CLASSIC4", 2}});
    picked.settle(true);
    CHECK(picked.filter() == LibraryFilter::Plus);
    CHECK(picked.choice() == FilterChoice::Picked);

    LibraryListing empty = library();
    empty.pick(LibraryFilter::Recent);
    empty.settle(false);
    CHECK(empty.filter() == LibraryFilter::All);
}

TEST(search_overrides_the_tab_but_not_the_platform)
{
    LibraryListing listing = library();
    listing.pick(LibraryFilter::Browse);
    listing.setQuery("later");
    CHECK(ids(listing.matching()) == (std::vector<std::string>{"LATER5"}));
    listing.setQuery("5");
    listing.setPlatform(PlatformFilter::PS4);
    CHECK(listing.matching().empty());
    listing.setQuery("");
    listing.pick(LibraryFilter::Plus);
    CHECK(ids(listing.matching()) == (std::vector<std::string>{"CLASSIC4"}));
}

TEST(filter_keys_round_trip_and_reject_the_old_modes)
{
    for (LibraryFilter filter : LibraryListing::allFilters)
        CHECK(LibraryListing::filterNamed(LibraryListing::key(filter)) == filter);
    CHECK(!LibraryListing::filterNamed("favorites"));
    CHECK(!LibraryListing::filterNamed(""));
}
