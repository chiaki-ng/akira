#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fixture_art.hpp"
#include "trophies/trophy_desk.hpp"

namespace tests {

class FakeTrophyDesk final : public akira::trophies::TrophyDesk {
public:
    FakeTrophyDesk()
        : titles{ title("NPWR20001_00", "Astro Bot", "astro", 78, { 17, 11, 6, 0 }, { 38, 20, 12, 1 }, 20,
                      { libchiaking::trophies::TrophyPlatform::ps5 }),
            title("NPWR20002_00", "Returnal", "returnal", 41, { 11, 5, 2, 0 }, { 25, 12, 6, 1 }, 18,
                { libchiaking::trophies::TrophyPlatform::ps5 }),
            title("NPWR20003_00", "Ghost of Tsushima", "ghost", 100, { 29, 13, 9, 1 }, { 29, 13, 9, 1 }, 15,
                { libchiaking::trophies::TrophyPlatform::ps4, libchiaking::trophies::TrophyPlatform::ps5 }),
            title("NPWR20004_00", "Gran Turismo 7", "gt7", 23, { 9, 4, 1, 0 }, { 30, 20, 8, 1 }, 12,
                { libchiaking::trophies::TrophyPlatform::ps4, libchiaking::trophies::TrophyPlatform::ps5 }),
            title("NPWR20005_00", "Stray", "stray", 62, { 8, 6, 3, 0 }, { 12, 9, 5, 1 }, 9,
                { libchiaking::trophies::TrophyPlatform::ps5 }) }
        , earned{ .level = 312, .tier = 4, .progress = 64, .earned = { .bronze = 627, .silver = 431, .gold = 212, .platinum = 14 } }
    {
    }

    auto linked() const -> bool override { return isLinked; }
    auto enabled() const -> bool override { return isEnabled; }

    void summary(bool, Answer<Summary> done) override { done(earned); }

    void library(bool force, Answer<std::vector<Title>> done) override
    {
        ++libraryCalls;
        forced = forced || force;
        if (failure)
        {
            done(std::unexpected(*failure));
            return;
        }
        done(titles);
    }

    void detail(const Title& shown, bool, Answer<Detail> done) override
    {
        opened.push_back(shown.id.text());
        done(detailOf(shown));
    }

    auto played(const TitleId& id) const -> std::optional<Played> override
    {
        if (id.text() != "NPWR20001_00")
            return std::nullopt;
        return Played{ .time = std::chrono::hours{ 14 } };
    }

    void resolvePlayed(const std::vector<Title>&, std::function<void()> updated) override { updated(); }

    auto cooldown(std::string_view scope) -> std::optional<std::chrono::seconds> override
    {
        if (!cooling.contains(std::string(scope)))
            return std::nullopt;
        return std::chrono::seconds{ 21540 };
    }

    void recordRefresh(std::string_view scope) override { cooling.insert(std::string(scope)); }

    auto savedAt() const -> std::int64_t override { return 0; }

    static auto detailOf(const Title& shown) -> Detail
    {
        using libchiaking::trophies::Grade;
        using libchiaking::trophies::Rarity;
        Detail made{ .title = shown.id };
        made.groups = { libchiaking::trophies::TrophyGroup{ .id = *libchiaking::trophies::TrophyGroupId::parse("default"),
                            .name = "Base game" },
            libchiaking::trophies::TrophyGroup{ .id = *libchiaking::trophies::TrophyGroupId::parse("001"), .name = "Winter Wonder" } };
        made.trophies = {
            trophy(1, "default", Grade::gold, "Bot Saver", "Rescue every bot in the galaxy.", "trophy-bot", true, Rarity::very_rare, 4.2),
            trophy(2, "default", Grade::silver, "Lost Galaxy", "Find the hidden planet.", "trophy-galaxy", true, Rarity::rare, 12.8),
            trophy(3, "default", Grade::gold, "Puzzle Pro", "Clear every puzzle level.", "trophy-puzzle", false, Rarity::ultra_rare, 2.1),
            trophy(4, "001", Grade::bronze, "Cosmic Secret", "Find the secret warp.", "trophy-secret", false, Rarity::common, 61.4),
            trophy(5, "default", Grade::bronze, "First Steps", "Finish the tutorial.", "trophy-first", true, Rarity::common, 96.0),
            trophy(6, "default", Grade::platinum, "Platinum Astro", "Earn every other trophy.", "trophy-platinum", false,
                Rarity::ultra_rare, 1.4),
        };
        made.trophies[2].progress_target = 30;
        made.trophies[2].progress = libchiaking::trophies::TrophyProgress{ .value = 23, .percent = 76 };
        made.trophies[3].hidden = true;
        return made;
    }

    std::vector<Title> titles;
    Summary earned;
    bool isLinked = true;
    bool isEnabled = true;
    bool forced = false;
    int libraryCalls = 0;
    std::optional<akira::accounts::Status> failure;
    std::vector<std::string> opened;
    std::set<std::string> cooling;

private:
    static auto title(const char* id, const char* name, const char* art, std::uint32_t progress,
        libchiaking::trophies::TrophyCounts got, libchiaking::trophies::TrophyCounts defined, int day,
        std::vector<libchiaking::trophies::TrophyPlatform> platforms) -> Title
    {
        Title made{ .id = *libchiaking::trophies::NpCommunicationId::parse(id) };
        made.name = name;
        made.icon_url = FixtureArt::url(art);
        made.progress = progress;
        made.earned = got;
        made.defined = defined;
        made.platforms = std::move(platforms);
        made.last_updated = libchiaking::trophies::Time{ std::chrono::days{ 20400 + day } };
        return made;
    }

    static auto trophy(int id, const char* group, libchiaking::trophies::Grade grade, const char* name, const char* detail,
        const char* art, bool got, libchiaking::trophies::Rarity rarity, double rate) -> libchiaking::trophies::Trophy
    {
        libchiaking::trophies::Trophy made{ .id = *libchiaking::trophies::TrophyId::of(id),
            .group = *libchiaking::trophies::TrophyGroupId::parse(group) };
        made.grade = grade;
        made.name = name;
        made.detail = detail;
        made.icon_url = FixtureArt::url(art);
        made.earned = got;
        made.rarity = rarity;
        made.earned_rate = rate;
        if (got)
            made.earned_at = libchiaking::trophies::Time{ std::chrono::days{ 20400 + id } };
        return made;
    }
};

}
