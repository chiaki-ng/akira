#include "test_util.hpp"

#include <chrono>
#include <string>
#include <vector>

#include "consoles/console_list.hpp"
#include "consoles/recent_play.hpp"
#include "consoles/ui/recent_shelf.hpp"
#include "screen_shots.hpp"

namespace {

using akira::consoles::ConsoleFacts;
using akira::consoles::ConsoleList;
using akira::consoles::HostType;
using akira::consoles::RecentPlay;

auto console(std::string name, bool ps5, bool standby) -> ConsoleFacts
{
    ConsoleFacts made;
    made.catalogName = name;
    made.name = std::move(name);
    made.type = HostType::Discovered;
    made.ps5 = ps5;
    made.paired = true;
    made.discovered = true;
    made.standby = standby;
    made.ready = !standby;
    made.hasAddress = true;
    made.presence = standby ? "Standby" : "Ready";
    return made;
}

auto played() -> RecentPlay
{
    const auto now = RecentPlay::Clock::now();
    const auto ago = [now](int hours) { return std::optional(now - std::chrono::hours(hours)); };
    const auto title = [](std::string id, std::string name, std::string art, std::optional<RecentPlay::Clock::time_point> used) {
        return RecentPlay::Title{ .titleId = std::move(id), .name = std::move(name), .coverUrl = tests::FixtureArt::url(art),
            .lastUsed = used, .recentlyPlayed = true };
    };
    return RecentPlay({ title("PPSA01325_00", "Astro Bot", "astro_bot", ago(1)),
        title("PPSA01284_00", "Returnal", "returnal", ago(30)),
        title("PPSA01521_00", "Ghost of Tsushima", "ghost_of_tsushima", ago(24 * 6)),
        title("PPSA01316_00", "Gran Turismo 7", "gran_turismo", ago(24 * 9)),
        title("PPSA01474_00", "Ratchet & Clank", "ratchet_and_clank", ago(24 * 16)),
        title("PPSA01403_00", "Sackboy", "sackboy", ago(24 * 40)),
        title("PPSA01530_00", "Stray", "stray", ago(24 * 70)) });
}

}

TEST(arisa_recent_shelf_finds_shown_titles)
{
    akira::consoles::ui::RecentShelf shelf;
    shelf.show("living-room", "Living room", played());
    CHECK(shelf.view().get().titles.size() == 7);
    CHECK(shelf.title("PPSA01284_00").has_value());
    shelf.clear();
    CHECK(shelf.view().get().titles.empty());
}

TEST(arisa_shot_consoles_recent)
{
    tests::ScreenShots shots;
    shots.shelf.show(ConsoleList({ console("Living room", true, false), console("Bedroom", false, true) },
        ConsoleList::Profile::Signed));
    akira::consoles::ui::RecentShelf recent;
    recent.show("Living room", "Living room", played());
    akira::app::arisa::ShellScreen::Parts parts = shots.parts();
    parts.recent = &recent;
    REQUIRE(shots.start(shots.shell(parts)));
    CHECK(shots.has("consoles.recent"));
    CHECK(shots.fixtures().requests() >= 5);
    CHECK(shots.write("consoles_recent"));
    REQUIRE(shots.press("consoles.recent.PPSA01284_00"));
    REQUIRE(shots.consoles.launched.size() == 1);
    CHECK(shots.consoles.launched.front().second == "PPSA01284_00");
}
