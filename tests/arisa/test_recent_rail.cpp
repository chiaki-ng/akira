#include "test_util.hpp"

#include <chrono>
#include <string>
#include <vector>

#include "consoles/console_list.hpp"
#include "consoles/recent_play.hpp"
#include "consoles/ui/recent_shelf.hpp"
#include "consoles/ui/console_card.hpp"
#include "play/ui/connecting_screen.hpp"
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

TEST(arisa_recent_shelf_gallery_follows_play_order_and_console)
{
    using Banner = akira::consoles::ui::RecentShelf::Banner;
    akira::consoles::ui::RecentShelf shelf;
    CHECK(shelf.loadingArt("Living room").empty());
    shelf.show("living-room", "Living room", played());
    shelf.exhibit(Banner{ .catalogName = "living-room", .titleId = "PPSA01521_00", .background = "ghost" });
    shelf.exhibit(Banner{ .catalogName = "living-room", .titleId = "PPSA01325_00", .background = "astro" });
    shelf.exhibit(Banner{ .catalogName = "living-room", .titleId = "PPSA01325_00", .background = "again" });
    shelf.exhibit(Banner{ .catalogName = "bedroom", .titleId = "PPSA01284_00", .background = "elsewhere" });
    const auto shown = shelf.loadingArt("Living room");
    CHECK(shown.size() == 2);
    CHECK(shown.front().background == "astro");
    CHECK(shown.back().background == "ghost");
    CHECK(shelf.loadingArt("Living room (Remote)").size() == 2);
    CHECK(shelf.loadingArt("Bedroom").empty());
    shelf.launch("PPSA01521_00");
    CHECK(shelf.loadingArt("Living room").size() == 1);
    CHECK(shelf.loadingArt("Living room").front().background == "ghost");
    shelf.launch("PPSA01284_00");
    CHECK(shelf.loadingArt("Living room").empty());
    shelf.landed();
    shelf.show("living-room", "Living room", played());
    CHECK(shelf.loadingArt("Living room").size() == 2);
}

TEST(arisa_shot_connecting_layered_title)
{
    using akira::play::ui::ConnectingScreen;
    const arisa::provider::SurfaceSize switch2{ .width = arisa::provider::SurfaceWidth{ 1920 },
        .height = arisa::provider::SurfaceHeight{ 1080 } };
    const ConnectingScreen::Progress progress{ .console = "Living room", .status = "Requesting the session", .step = 3, .steps = 6 };
    const arisa::provider::SurfaceSize tv{ .width = arisa::provider::SurfaceWidth{ 3840 },
        .height = arisa::provider::SurfaceHeight{ 2160 } };
    const auto shoot = [&](ConnectingScreen::Art art, std::string_view name) {
        {
            tests::ScreenShots shots;
            REQUIRE(shots.start(ConnectingScreen(progress, shots.stream, shots.text(), { art }), switch2));
            REQUIRE(shots.decodeLargeArt());
            CHECK(shots.write(name));
        }
        tests::ScreenShots docked;
        REQUIRE(docked.start(ConnectingScreen(progress, docked.stream, docked.text(), { art }), tv,
            arisa::provider::DisplayMode::docked));
        REQUIRE(docked.decodeLargeArt());
        CHECK(docked.write(std::string(name) + "_tv"));
    };
    shoot(ConnectingScreen::Art{ .title = "Marvel's Wolverine", .background = tests::FixtureArt::url("backdrop_wolverine"),
              .hero = tests::FixtureArt::url("hero_wolverine"), .logo = tests::FixtureArt::url("logo_wolverine") },
        "connecting_layered_hero");
    shoot(ConnectingScreen::Art{ .title = "Marvel's Spider-Man 2", .background = tests::FixtureArt::url("backdrop_wolverine") },
        "connecting_layered_text");
}

TEST(arisa_shot_card_hero)
{
    const arisa::provider::SurfaceSize switch2{ .width = arisa::provider::SurfaceWidth{ 1920 },
        .height = arisa::provider::SurfaceHeight{ 1080 } };
    tests::ScreenShots shots;
    shots.shelf.show(ConsoleList({ console("PS5-054", true, false) }, ConsoleList::Profile::Signed));
    const akira::consoles::ui::ConsoleShelf::Card card = shots.shelf.view().get().cards.front();
    const akira::consoles::ui::RecentShelf::Banner banner{ .catalogName = card.catalogName, .titleId = "PPSA00000_00",
        .background = tests::FixtureArt::url("backdrop_wolverine"), .hero = tests::FixtureArt::url("hero_wolverine"),
        .logo = tests::FixtureArt::url("logo_wolverine") };
    arisa::composition::Element shown
        = arisa::composition::Element(akira::consoles::ui::ConsoleCard(card, false, shots.consoles, shots.text(), std::nullopt, banner))
              .frame({ .width = 1716.0F })
              .padding(arisa::theme::Space{ 100.0F })
              .frame({ .max_width = arisa::composition::infinity, .max_height = arisa::composition::infinity,
                  .alignment = arisa::composition::Alignment::top_leading });
    REQUIRE(shots.start(std::move(shown), switch2));
    REQUIRE(shots.decodeLargeArt());
    CHECK(shots.write("card_hero"));
}
