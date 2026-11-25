#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cloud/fetch_progress.hpp"
#include "cloud/game_actions.hpp"
#include "cloud/launch_checks.hpp"
#include "cloud/models.hpp"
#include "cloud/service.hpp"
#include "cloud/title_refresh.hpp"

namespace akira::cloud {

class CloudLaunch;

class CloudDesk {
public:
    using Snapshotted = std::function<void(const Snapshot&)>;
    using Fetched = std::function<void(const FetchProgress&)>;
    using Detailed = std::function<void(std::optional<GameDetail>)>;
    using Checked = std::function<void(CardCheck)>;
    using Refreshed = std::function<void(TitleRefresh)>;
    using Cleared = std::function<void(std::shared_ptr<CloudLaunch>)>;
    using Refused = std::function<void(std::string)>;
    using Observed = std::function<void(LaunchChecks::Check)>;

    virtual ~CloudDesk() = default;

    virtual auto snapshot() const -> Snapshot = 0;
    virtual auto snapshotOf(const FetchFailure& failure) const -> Snapshot = 0;
    virtual void refresh(bool force, Snapshotted done, Fetched fetched) = 0;
    virtual void clearCatalogCache() = 0;

    virtual void details(const Game& game, Detailed done) = 0;
    virtual void checkStorefront(const Game& game, Checked done) = 0;
    virtual void refreshTitle(const Game& game, Refreshed done) = 0;
    virtual void clearForLaunch(const Game& game, Cleared cleared, Refused refused, Observed observed) = 0;
    virtual void cancelLaunch() = 0;
    virtual void launch(std::shared_ptr<CloudLaunch> cleared) = 0;
    virtual void signIn() = 0;

    virtual auto signedIn() const -> bool = 0;
    virtual auto nowMs() const -> std::int64_t = 0;

    virtual auto shortcuts() const -> std::vector<Game> = 0;
    virtual void setShortcuts(std::vector<Game> games) = 0;
    virtual auto filter() const -> std::string = 0;
    virtual void setFilter(std::string key) = 0;
    virtual auto sortState() const -> int = 0;
    virtual void setSortState(int state) = 0;
};

}
