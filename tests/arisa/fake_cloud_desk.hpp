#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "cloud/cloud_desk.hpp"
#include "fixture_art.hpp"

namespace tests {

class FakeCloudDesk final : public akira::cloud::CloudDesk {
public:
    using Game = akira::cloud::Game;

    static constexpr std::int64_t now = 1'795'000'000'000;

    FakeCloudDesk()
        : listed(ready())
    {
    }

    static auto game(std::string id, std::string name, std::string cover, std::string platform, bool owned) -> Game
    {
        Game made;
        made.productId = std::move(id);
        made.name = std::move(name);
        made.imageUrl = FixtureArt::url(cover);
        made.landscapeImageUrl = FixtureArt::url("backdrop_" + cover);
        made.platform = std::move(platform);
        made.isOwned = owned;
        made.category = owned ? "owned" : "streamable";
        made.serviceType = made.platform == "ps5" ? "pscloud" : "psnow";
        made.streamServiceType = made.serviceType;
        made.plusCatalog = !owned;
        made.conceptUrl = "https://store.playstation.com/concept/10001234";
        made.streamabilityStatus = akira::cloud::StreamabilityStatus::Streamable;
        return made;
    }

    static auto catalog() -> std::vector<Game>
    {
        return {
            game("UP9000-PPSA01325_00-ASTROBOT00000000", "Astro Bot", "astro_bot", "ps5", true),
            game("UP9000-PPSA01342_00-RETURNAL00000000", "Returnal", "returnal", "ps5", false),
            game("UP9000-PPSA01450_00-GHOSTOFTSUSHIMA0", "Ghost of Tsushima", "ghost_of_tsushima", "ps5", false),
            game("UP9000-PPSA01321_00-GRANTURISMO70000", "Gran Turismo 7", "gran_turismo", "ps5", true),
            game("UP9000-CUSA02378_00-TEARAWAYUNFOLDED", "Tearaway Unfolded", "tearaway", "ps4", false),
            game("UP9000-PPSA01474_00-RATCHETANDCLANK0", "Ratchet & Clank", "ratchet_and_clank", "ps5", true),
            game("UP9000-PPSA01511_00-SACKBOYADVENTURE", "Sackboy", "sackboy", "ps5", false),
            game("UP9000-PPSA01580_00-DEATHSTRANDING00", "Death Stranding", "death_stranding", "ps5", true),
            game("UP9000-PPSA01521_00-HORIZONFORBIDDEN", "Horizon Forbidden West", "horizon", "ps5", false),
            game("UP9000-PPSA01341_00-DEMONSSOULS00000", "Demon's Souls", "demons_souls", "ps5", true),
        };
    }

    static auto ready() -> akira::cloud::Snapshot
    {
        akira::cloud::Snapshot made;
        made.status.availability = akira::cloud::Availability::Ready;
        made.status.title = "10 cloud games ready";
        made.status.gameCount = 10;
        made.status.canBrowse = true;
        made.catalog.games = catalog();
        made.recentlyPlayed = { akira::cloud::RecentlyPlayed{ .productId = made.catalog.games[0].productId, .lastPlayedMs = now - 86'400'000 } };
        made.hasCatalog = true;
        return made;
    }

    static auto unpaired() -> akira::cloud::Snapshot
    {
        akira::cloud::Snapshot made;
        made.status.availability = akira::cloud::Availability::NeedsPairing;
        made.status.title = "Cloud needs pairing";
        made.status.detail = "Sign this profile in to PlayStation to enable cloud streaming.";
        made.status.canPair = true;
        return made;
    }

    static auto facts() -> akira::cloud::GameDetail
    {
        akira::cloud::GameDetail made;
        made.art = akira::cloud::GameDetail::Art::GameHub;
        made.background = FixtureArt::url("backdrop_astro_bot");
        made.rating = akira::cloud::GameDetail::Rating{ .average = 4.8,
            .averageText = "4.8",
            .count = 128'412,
            .percentByStars = { 1, 1, 3, 12, 83 } };
        made.age = akira::cloud::GameDetail::Age{ .name = "PEGI 7", .descriptors = "Mild violence" };
        made.publisher = "Sony Interactive Entertainment";
        made.releasedMs = 1'725'580'800'000;
        made.genres = { "Action", "Platformer" };
        return made;
    }

    auto snapshot() const -> akira::cloud::Snapshot override { return listed; }
    auto snapshotOf(const akira::cloud::FetchFailure&) const -> akira::cloud::Snapshot override { return failed; }
    void refresh(bool force, Snapshotted done, Fetched fetched) override
    {
        ++refreshes;
        forced = forced || force;
        if (progress)
            fetched(*progress);
        if (answers)
            done(listed);
    }
    void clearCatalogCache() override { ++cacheClears; }

    void details(const Game&, Detailed done) override { done(facts()); }
    void checkStorefront(const Game&, Checked done) override
    {
        if (holdsCardCheck)
            heldCardCheck = std::move(done);
        else
            done(akira::cloud::CardCheck::streams());
    }
    void refreshTitle(const Game& asked, Refreshed done) override
    {
        Game added = asked;
        added.needsLibraryAdd = false;
        done(akira::cloud::TitleRefresh{ .outcome = akira::cloud::TitleRefresh::Outcome::Added, .game = added });
    }
    void clearForLaunch(const Game& asked, Cleared clear, Refused refuse, Observed observe) override
    {
        clearing = asked.productId;
        for (const akira::cloud::LaunchChecks::Check check : observed)
            observe(check);
        if (holdsLaunch)
        {
            heldClear = std::move(clear);
            heldRefuse = std::move(refuse);
        }
        else if (refusal)
            refuse(*refusal);
        else if (clears)
            clear(nullptr);
    }
    void cancelLaunch() override { ++cancels; }
    void launch(std::shared_ptr<akira::cloud::CloudLaunch>) override { ++launches; }
    void signIn() override { ++signIns; }

    auto signedIn() const -> bool override { return true; }
    auto nowMs() const -> std::int64_t override { return now; }

    auto shortcuts() const -> std::vector<Game> override { return pinned; }
    void setShortcuts(std::vector<Game> games) override { pinned = std::move(games); }
    auto filter() const -> std::string override { return savedFilter; }
    void setFilter(std::string key) override { savedFilter = std::move(key); }
    auto sortState() const -> int override { return savedSort; }
    void setSortState(int state) override { savedSort = state; }

    akira::cloud::Snapshot listed;
    akira::cloud::Snapshot failed = unpaired();
    std::optional<akira::cloud::FetchProgress> progress;
    bool answers = true;
    bool clears = true;
    bool holdsLaunch = false;
    bool holdsCardCheck = false;
    Cleared heldClear;
    Refused heldRefuse;
    Checked heldCardCheck;
    std::optional<std::string> refusal;
    std::vector<akira::cloud::LaunchChecks::Check> observed;
    std::vector<Game> pinned;
    std::string savedFilter;
    int savedSort = 0;
    std::string clearing;
    int refreshes = 0;
    bool forced = false;
    int cacheClears = 0;
    int cancels = 0;
    int launches = 0;
    int signIns = 0;
};

}
