#include "test_util.hpp"

#include <chrono>
#include <string>
#include <vector>

#include "trophies/title_listing.hpp"
#include "trophies/trophy_listing.hpp"

namespace {

using akira::trophies::TitleListing;
using akira::trophies::TrophyListing;
namespace lib = libchiaking::trophies;

auto titleOf(const char* id, const char* name, std::uint32_t progress, std::uint32_t earned, std::uint32_t defined,
    int day, bool hidden = false) -> lib::TrophyTitle
{
    lib::TrophyTitle made{ .id = *lib::NpCommunicationId::parse(id) };
    made.name = name;
    made.progress = progress;
    made.earned.bronze = earned;
    made.defined.bronze = defined;
    made.hidden = hidden;
    made.last_updated = lib::Time{ std::chrono::days{ day } };
    return made;
}

auto trophyOf(int id, const char* group, lib::Grade grade, const char* name, bool earned) -> lib::Trophy
{
    lib::Trophy made{ .id = *lib::TrophyId::of(id), .group = *lib::TrophyGroupId::parse(group) };
    made.grade = grade;
    made.name = name;
    made.earned = earned;
    return made;
}

auto names(const std::vector<lib::TrophyTitle>& titles) -> std::vector<std::string>
{
    std::vector<std::string> listed;
    for (const auto& title : titles)
        listed.push_back(title.name);
    return listed;
}

auto names(const std::vector<lib::Trophy>& trophies) -> std::vector<std::string>
{
    std::vector<std::string> listed;
    for (const auto& trophy : trophies)
        listed.push_back(trophy.name);
    return listed;
}

auto library() -> std::vector<lib::TrophyTitle>
{
    return { titleOf("NPWR00001_00", "Returnal", 41, 18, 40, 10), titleOf("NPWR00002_00", "Astro Bot", 78, 34, 44, 20),
        titleOf("NPWR00003_00", "Stray", 0, 0, 20, 5), titleOf("NPWR00004_00", "Ghost", 100, 52, 52, 15),
        titleOf("NPWR00005_00", "Secret", 10, 1, 10, 30, true) };
}

}

TEST(trophy_titles_default_to_recent_activity_without_hidden)
{
    CHECK((names(TitleListing().listed(library())) == std::vector<std::string>{ "Astro Bot", "Ghost", "Returnal", "Stray" }));
}

TEST(trophy_titles_sort_by_progress_then_recency)
{
    const TitleListing listing(TitleListing::Sort::Progress, TitleListing::Filter::All);
    CHECK((names(listing.listed(library())) == std::vector<std::string>{ "Ghost", "Astro Bot", "Returnal", "Stray" }));
}

TEST(trophy_titles_filter_like_borealis)
{
    const auto shown = [](TitleListing::Filter filter) {
        return names(TitleListing(TitleListing::Sort::Name, filter).listed(library()));
    };
    CHECK((shown(TitleListing::Filter::InProgress) == std::vector<std::string>{ "Astro Bot", "Returnal" }));
    CHECK((shown(TitleListing::Filter::Completed) == std::vector<std::string>{ "Ghost" }));
    CHECK((shown(TitleListing::Filter::Unstarted) == std::vector<std::string>{ "Stray" }));
    CHECK((shown(TitleListing::Filter::Hidden) == std::vector<std::string>{ "Secret" }));
}

TEST(trophy_titles_sort_by_earned)
{
    const TitleListing listing(TitleListing::Sort::Earned, TitleListing::Filter::All);
    CHECK((names(listing.listed(library())) == std::vector<std::string>{ "Ghost", "Astro Bot", "Returnal", "Stray" }));
}

TEST(trophy_list_groups_filters_and_sorts)
{
    lib::TitleTrophies detail{ .title = *lib::NpCommunicationId::parse("NPWR00002_00") };
    detail.groups.push_back(lib::TrophyGroup{ .id = *lib::TrophyGroupId::parse("default"), .name = "Base game" });
    detail.groups.push_back(lib::TrophyGroup{ .id = *lib::TrophyGroupId::parse("001"), .name = "Winter" });
    detail.trophies = { trophyOf(1, "default", lib::Grade::bronze, "First", true),
        trophyOf(2, "default", lib::Grade::platinum, "Platinum", false), trophyOf(3, "001", lib::Grade::gold, "Snow", true),
        trophyOf(4, "default", lib::Grade::gold, "Bots", false) };

    CHECK((names(TrophyListing(TrophyListing::Sort::Grade, TrophyListing::Filter::All, std::nullopt).listed(detail))
        == std::vector<std::string>{ "Platinum", "Snow", "Bots", "First" }));
    CHECK((names(TrophyListing(TrophyListing::Sort::Name, TrophyListing::Filter::All, lib::TrophyGroupId::parse("001"))
                     .listed(detail))
        == std::vector<std::string>{ "Snow" }));
    CHECK((names(TrophyListing(TrophyListing::Sort::Progress, TrophyListing::Filter::Earned, std::nullopt).listed(detail))
        == std::vector<std::string>{ "First", "Snow" }));
    CHECK((names(TrophyListing(TrophyListing::Sort::Progress, TrophyListing::Filter::All, std::nullopt).listed(detail))
        == std::vector<std::string>{ "Platinum", "Bots", "First", "Snow" }));
}

TEST(trophy_rarity_sorts_rated_rarest_first)
{
    lib::TitleTrophies detail{ .title = *lib::NpCommunicationId::parse("NPWR00002_00") };
    detail.trophies = { trophyOf(1, "default", lib::Grade::bronze, "Common", true),
        trophyOf(2, "default", lib::Grade::gold, "Unrated", false), trophyOf(3, "default", lib::Grade::gold, "Rare", false) };
    detail.trophies[0].earned_rate = 61.4;
    detail.trophies[2].earned_rate = 2.1;
    CHECK((names(TrophyListing(TrophyListing::Sort::Rarity, TrophyListing::Filter::All, std::nullopt).listed(detail))
        == std::vector<std::string>{ "Rare", "Common", "Unrated" }));
}

TEST(trophy_keys_match_the_catalog)
{
    CHECK(TrophyListing::rarityKey(lib::Rarity::very_rare) == "akira/trophies/rarity_very_rare");
    CHECK(TrophyListing::gradeAsset(lib::Grade::gold) == "img/trophy/gold.png");
    CHECK(TitleListing::sortKey(TitleListing::Sort::Recent) == "akira/trophies/tsort_recent");
    CHECK(TitleListing::platformName(lib::TrophyPlatform::ps_vita) == "PS Vita");
}
