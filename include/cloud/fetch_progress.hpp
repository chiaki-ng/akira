#ifndef AKIRA_CLOUD_FETCH_PROGRESS_HPP
#define AKIRA_CLOUD_FETCH_PROGRESS_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cloud {

class FetchFailure {
public:
    enum class Reason {
        Cancelled,
        User,
        Unavailable,
        Rejected,
        RegionUnsupported,
        Malformed,
        NotListed,
        Storage,
        OwnershipUnknown,
        RestrictionListUnavailable
    };

    FetchFailure(Reason reason, uint16_t httpStatus, bool needsSignIn);

    auto reason() const -> Reason { return why; }
    auto httpStatus() const -> uint16_t { return status; }
    bool needsSignIn() const { return signIn; }

    friend bool operator==(const FetchFailure&, const FetchFailure&) = default;

private:
    Reason why;
    uint16_t status;
    bool signIn;
};

class FetchProgress {
public:
    enum class Step {
        Idle,
        CheckingSignIn,
        CloudList,
        OwnedLibrary,
        PremiumCategory,
        ForSaleCategory,
        StreamingAccess,
        Matching,
        TrustedTime,
        Exclusions,
        Ready,
        Unanswered,
        Cancelled
    };

    enum class Activity {
        ReusingHeldAnswer,
        WalkingPublicStore,
        WalkingOldServiceShelves,
        WalkingPublicStoreFallback,
        CheckingStreamingCopies,
        ReadingPs5StoreLists,
        ReadingEntitlements,
        ReadingStreamedPs4Concepts,
        ScanningPs5Library,
        ReadingCategoryPages,
        ReadingSubscriptions,
        MatchingGames,
        SyncingClock,
        ReadingExclusions
    };

    enum class Outcome {
        Skipped,
        Held,
        Fetched,
        Degraded
    };

    struct PassedStep {
        Step step = Step::CheckingSignIn;
        Outcome outcome = Outcome::Skipped;

        friend bool operator==(const PassedStep&, const PassedStep&) = default;
    };

    enum class Shown {
        Nothing,
        Lines,
        Failure
    };

    FetchProgress() = default;
    FetchProgress(uint64_t attempt, Step step, std::optional<Activity> activity, uint32_t pagesRead,
                  std::optional<uint32_t> pagesExpected, std::vector<PassedStep> passed,
                  std::optional<FetchFailure> failure, std::optional<std::string> timeServer = std::nullopt);

    auto attempt() const -> uint64_t { return attemptId; }
    auto step() const -> Step { return reached; }
    auto activity() const -> std::optional<Activity> { return doing; }
    auto pagesRead() const -> uint32_t { return read; }
    auto pagesExpected() const -> std::optional<uint32_t> { return expected; }
    auto passed() const -> const std::vector<PassedStep>& { return steps; }
    auto failure() const -> const std::optional<FetchFailure>& { return failed; }
    auto timeServer() const -> const std::optional<std::string>& { return server; }
    bool isRunning() const;
    bool begunAfter(std::optional<uint64_t> superseded) const;

    auto shown() const -> Shown;
    auto stepKey() const -> std::optional<std::string_view>;
    auto detailKey() const -> std::optional<std::string_view>;
    auto pageKey() const -> std::optional<std::string_view>;

    friend bool operator==(const FetchProgress&, const FetchProgress&) = default;

private:
    static auto keyOf(Step step) -> std::optional<std::string_view>;
    static auto keyOf(Activity activity) -> std::optional<std::string_view>;

    uint64_t attemptId = 0;
    Step reached = Step::Idle;
    std::optional<Activity> doing;
    uint32_t read = 0;
    std::optional<uint32_t> expected;
    std::vector<PassedStep> steps;
    std::optional<FetchFailure> failed;
    std::optional<std::string> server;
};

class FollowedFetch {
public:
    enum class Change {
        Dropped,
        Unchanged,
        Lines,
        Step
    };

    void follow(uint64_t operation);
    auto observe(uint64_t operation, FetchProgress progress) -> Change;

    auto operation() const -> uint64_t { return followed; }
    auto latest() const -> const std::optional<FetchProgress>& { return current; }

private:
    uint64_t followed = 0;
    std::optional<FetchProgress> current;
};

} // namespace cloud

#endif // AKIRA_CLOUD_FETCH_PROGRESS_HPP
