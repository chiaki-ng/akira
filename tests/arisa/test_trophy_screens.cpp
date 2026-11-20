#include "test_util.hpp"

#include <map>
#include <string>

#include "fake_strings.hpp"
#include "fake_trophy_desk.hpp"
#include "screen_shots.hpp"
#include "trophies/ui/trophies_screen.hpp"
#include "trophies/ui/trophy_model.hpp"

namespace {

using akira::app::arisa::ShellModel;
using akira::trophies::TitleListing;
using akira::trophies::TrophyListing;
using akira::trophies::ui::TrophyModel;

auto quietStrings() -> tests::FakeStrings
{
    return tests::FakeStrings({ { "akira/trophies/force_refresh", "Refreshing trophies from PSN" } });
}

class TrophyShots {
public:
    TrophyShots()
        : model(desk, shots.collected, shots.text())
    {
        shots.model.offer(false, true);
        shots.model.show(ShellModel::Page::Trophies);
    }

    auto start() -> bool
    {
        auto parts = shots.parts();
        parts.trophies = &model;
        return shots.start(shots.shell(parts));
    }

    tests::FakeTrophyDesk desk;
    tests::ScreenShots shots;
    TrophyModel model;
};

}

TEST(trophy_model_loads_on_appear_and_lists_titles)
{
    tests::FakeTrophyDesk desk;
    tests::CollectedNotices notices;
    const tests::FakeStrings strings = quietStrings();
    TrophyModel model(desk, notices, strings);
    model.appear();
    const TrophyModel::View& view = model.view().get();
    CHECK(view.phase == TrophyModel::Phase::Ready);
    CHECK(view.rows.size() == 5);
    CHECK(view.rows.front().title.name == "Astro Bot");
    CHECK(!view.rows.front().played.empty());
    CHECK(view.summary.has_value() && view.summary->level == 312);
    model.appear();
    CHECK(desk.libraryCalls == 1);
}

TEST(trophy_model_gates_on_link_and_profile)
{
    tests::FakeTrophyDesk desk;
    tests::CollectedNotices notices;
    const tests::FakeStrings strings = quietStrings();
    TrophyModel model(desk, notices, strings);
    desk.isLinked = false;
    model.appear();
    CHECK(model.view().get().phase == TrophyModel::Phase::NotLinked);
    CHECK(desk.libraryCalls == 0);
    desk.isLinked = true;
    desk.isEnabled = false;
    model.appear();
    CHECK(model.view().get().phase == TrophyModel::Phase::Disabled);
    desk.isEnabled = true;
    model.appear();
    CHECK(model.view().get().phase == TrophyModel::Phase::Ready);
}

TEST(trophy_model_failure_without_titles_shows_the_error)
{
    tests::FakeTrophyDesk desk;
    tests::CollectedNotices notices;
    const tests::FakeStrings strings = quietStrings();
    desk.failure = akira::accounts::Status::Offline;
    TrophyModel model(desk, notices, strings);
    model.appear();
    CHECK(model.view().get().phase == TrophyModel::Phase::Failed);
    CHECK(model.view().get().error == "akira/trophies/error_offline");
}

TEST(trophy_model_refresh_starts_the_cooldown)
{
    tests::FakeTrophyDesk desk;
    tests::CollectedNotices notices;
    const tests::FakeStrings strings = quietStrings();
    TrophyModel model(desk, notices, strings);
    model.appear();
    model.refresh();
    CHECK(desk.forced);
    CHECK(desk.libraryCalls == 2);
    CHECK(notices.shown.size() == 1);
    CHECK(model.view().get().wait.has_value());
    model.refresh();
    CHECK(desk.libraryCalls == 2);
    CHECK(TrophyModel::waitShown(21540) == "5h 59m");
}

TEST(trophy_model_sorts_filters_and_opens_a_game)
{
    tests::FakeTrophyDesk desk;
    tests::CollectedNotices notices;
    const tests::FakeStrings strings = quietStrings();
    TrophyModel model(desk, notices, strings);
    model.appear();
    model.setSort(TitleListing::Sort::Progress);
    CHECK(model.view().get().rows.front().title.name == "Ghost of Tsushima");
    model.setFilter(TitleListing::Filter::Completed);
    CHECK(model.view().get().rows.size() == 1);
    model.setFilter(TitleListing::Filter::All);
    model.open("NPWR20001_00");
    const auto& page = model.view().get().page;
    CHECK(page.has_value() && !page->loading);
    CHECK(page->groups.size() == 2);
    CHECK(page->trophies.size() == 6);
    model.selectGroup("001");
    CHECK(model.view().get().page->trophies.size() == 1);
    model.selectGroup(std::nullopt);
    model.setTrophyFilter(TrophyListing::Filter::Earned);
    CHECK(model.view().get().page->trophies.size() == 3);
    model.setTrophyFilter(TrophyListing::Filter::All);
    model.setTrophySort(TrophyListing::Sort::Grade);
    CHECK(model.view().get().page->trophies.front().name == "Platinum Astro");
    model.reveal(4);
    CHECK(model.view().get().page->revealed.contains(4));
    model.showTrophy(1);
    CHECK(model.view().get().page->shown.has_value());
    model.closeTrophy();
    model.close();
    CHECK(!model.view().get().page.has_value());
}

TEST(arisa_shot_trophies)
{
    TrophyShots trophies;
    REQUIRE(trophies.start());
    CHECK(trophies.shots.has("trophies.summary"));
    CHECK(trophies.shots.write("trophies"));
}

TEST(arisa_shot_trophies_game)
{
    TrophyShots trophies;
    REQUIRE(trophies.start());
    REQUIRE(trophies.shots.press("trophies.title.NPWR20001_00"));
    CHECK(trophies.shots.has("trophies.page.rows"));
    CHECK(trophies.shots.write("trophies_game"));
}

TEST(arisa_shot_trophies_game_filtered)
{
    TrophyShots trophies;
    REQUIRE(trophies.start());
    trophies.model.open("NPWR20001_00");
    trophies.model.selectGroup("default");
    trophies.model.setTrophyFilter(TrophyListing::Filter::Unearned);
    trophies.model.setTrophySort(TrophyListing::Sort::Rarity);
    REQUIRE(trophies.shots.settle() && trophies.shots.settleArt());
    CHECK(trophies.shots.write("trophies_game_filtered"));
}

TEST(arisa_shot_trophies_detail)
{
    TrophyShots trophies;
    REQUIRE(trophies.start());
    trophies.model.open("NPWR20001_00");
    trophies.model.showTrophy(1);
    REQUIRE(trophies.shots.settle() && trophies.shots.settleArt());
    CHECK(trophies.shots.has("trophies.sheet"));
    CHECK(trophies.shots.write("trophies_detail"));
}

TEST(arisa_shot_trophies_not_linked)
{
    TrophyShots trophies;
    trophies.desk.isLinked = false;
    REQUIRE(trophies.start());
    CHECK(trophies.shots.has("trophies.not-linked"));
    CHECK(trophies.shots.write("trophies_not_linked"));
}
