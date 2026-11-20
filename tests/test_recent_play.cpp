#include "test_util.hpp"

#include "consoles/recent_play.hpp"

#include <chrono>
#include <string>
#include <vector>

using akira::consoles::RecentPlay;

namespace {

auto title(std::string id, std::optional<RecentPlay::Clock::time_point> used, bool recent) -> RecentPlay::Title
{
    return RecentPlay::Title{ .titleId = id, .name = id, .coverUrl = "https://art.test/" + id + ".png",
        .lastUsed = used, .recentlyPlayed = recent };
}

}

TEST(recent_play_keeps_recently_played_titles_newest_first)
{
    const auto now = RecentPlay::Clock::now();
    const RecentPlay played({ title("PPSA1", now - std::chrono::hours(48), true), title("PPSA2", std::nullopt, false),
        title("PPSA3", now - std::chrono::hours(2), true), title("PPSA4", std::nullopt, true) });
    REQUIRE(played.titles().size() == 3);
    CHECK(played.titles()[0].titleId == "PPSA3");
    CHECK(played.titles()[1].titleId == "PPSA1");
    CHECK(played.titles()[2].titleId == "PPSA4");
    CHECK(played.find("PPSA2") == std::nullopt);
}

TEST(recent_play_falls_back_to_installed_titles)
{
    const RecentPlay played({ title("PPSA1", std::nullopt, false), title("PPSA2", std::nullopt, false) });
    CHECK(played.titles().size() == 2);
}

TEST(recent_play_caps_the_rail)
{
    std::vector<RecentPlay::Title> many;
    for (int index = 0; index < 20; ++index)
        many.push_back(title("PPSA" + std::to_string(index), std::nullopt, true));
    CHECK(RecentPlay(std::move(many)).titles().size() == RecentPlay::limit);
}

TEST(recent_play_ages_by_calendar_day)
{
    using std::chrono::days;
    using std::chrono::hours;
    const RecentPlay::Clock::time_point noon = std::chrono::sys_days{ std::chrono::year{ 2026 } / 11 / 20 } + hours(12);
    CHECK(RecentPlay::age(noon - hours(3), noon) == (RecentPlay::Age{ .span = RecentPlay::Span::Today }));
    CHECK(RecentPlay::age(noon - hours(13), noon) == (RecentPlay::Age{ .span = RecentPlay::Span::Yesterday, .count = 1 }));
    CHECK(RecentPlay::age(noon - days(3), noon) == (RecentPlay::Age{ .span = RecentPlay::Span::Days, .count = 3 }));
    CHECK(RecentPlay::age(noon - days(15), noon) == (RecentPlay::Age{ .span = RecentPlay::Span::Weeks, .count = 2 }));
    CHECK(RecentPlay::age(noon - days(65), noon) == (RecentPlay::Age{ .span = RecentPlay::Span::Months, .count = 2 }));
    CHECK(RecentPlay::age(noon - days(800), noon) == (RecentPlay::Age{ .span = RecentPlay::Span::Years, .count = 2 }));
    CHECK(std::string(RecentPlay::ageKey(RecentPlay::Span::Days)) == "akira/hosts/recent_days");
}
