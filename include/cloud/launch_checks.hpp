#ifndef AKIRA_CLOUD_LAUNCH_CHECKS_HPP
#define AKIRA_CLOUD_LAUNCH_CHECKS_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "cloud/models.hpp"

namespace cloud {

class LaunchChecks {
public:
    enum class Step {
        Membership,
        Premium,
        YourCopy,
        CloudGame,
        Restrictions,
        Release
    };

    enum class Check {
        LoadingLibrary,
        Membership,
        CloudStreaming,
        Ownership,
        Entitlement,
        CloudGame,
        Exclusions,
        Release
    };

    enum class Mark {
        Unchecked,
        Checking,
        Passed,
        Refused
    };

    enum class Phase {
        Starting,
        Checking,
        Cleared,
        Launched,
        Refused
    };

    LaunchChecks(std::string productId, uint64_t attempt, const Game& game);

    void observe(Check check);
    void clear();
    void refuse(std::string reason);
    void launched();

    auto productId() const -> const std::string& { return product; }
    auto attempt() const -> uint64_t { return attemptId; }
    auto steps() const -> const std::vector<Step>& { return row; }
    auto phase() const -> Phase { return current; }
    auto reason() const -> const std::optional<std::string>& { return refusal; }
    bool isRunning() const { return current == Phase::Starting || current == Phase::Checking; }
    auto mark(Step step) const -> Mark;

    static auto stepOf(Check check) -> std::optional<Step>;
    static auto rowFor(const Game& game) -> std::vector<Step>;

private:
    auto indexOf(Step step) const -> std::optional<std::size_t>;

    std::string product;
    uint64_t attemptId;
    std::vector<Step> row;
    Phase current = Phase::Starting;
    std::optional<Step> reached;
    std::optional<std::string> refusal;
};

} // namespace cloud

#endif // AKIRA_CLOUD_LAUNCH_CHECKS_HPP
