#include "cloud/fetch_progress.hpp"

#include <utility>

namespace cloud {

FetchFailure::FetchFailure(Reason reason, uint16_t httpStatus, bool needsSignIn)
    : why(reason), status(httpStatus), signIn(needsSignIn)
{
}

FetchProgress::FetchProgress(uint64_t attempt, Step step, std::optional<Activity> activity, uint32_t pagesRead,
                             std::optional<uint32_t> pagesExpected, std::vector<PassedStep> passed,
                             std::optional<FetchFailure> failure, std::optional<std::string> timeServer)
    : attemptId(attempt), reached(step), doing(activity), read(pagesRead), expected(pagesExpected),
      steps(std::move(passed)), failed(std::move(failure)), server(std::move(timeServer))
{
}

bool FetchProgress::isRunning() const
{
    switch (reached)
    {
        case Step::Idle:
        case Step::Ready:
        case Step::Unanswered:
        case Step::Cancelled:
            return false;
        default:
            return true;
    }
}

bool FetchProgress::begunAfter(std::optional<uint64_t> superseded) const
{
    return !superseded || attemptId > *superseded;
}

auto FetchProgress::shown() const -> Shown
{
    if (reached == Step::Unanswered && failed)
        return Shown::Failure;
    if (isRunning())
        return Shown::Lines;
    return Shown::Nothing;
}

auto FetchProgress::stepKey() const -> std::optional<std::string_view>
{
    if (shown() != Shown::Lines)
        return std::nullopt;
    return keyOf(reached);
}

auto FetchProgress::detailKey() const -> std::optional<std::string_view>
{
    if (shown() != Shown::Lines || !doing)
        return std::nullopt;
    if (*doing == Activity::SyncingClock && server)
        return "akira/cloud/fetch_activity_syncing_clock_with";
    return keyOf(*doing);
}

auto FetchProgress::pageKey() const -> std::optional<std::string_view>
{
    if (shown() != Shown::Lines || !doing || read == 0)
        return std::nullopt;
    if (expected)
        return "akira/cloud/fetch_page_of";
    return "akira/cloud/fetch_page";
}

auto FetchProgress::keyOf(Step step) -> std::optional<std::string_view>
{
    switch (step)
    {
        case Step::CheckingSignIn: return "akira/cloud/fetch_step_checking_sign_in";
        case Step::CloudList: return "akira/cloud/fetch_step_cloud_list";
        case Step::OwnedLibrary: return "akira/cloud/fetch_step_owned_library";
        case Step::PremiumCategory: return "akira/cloud/fetch_step_premium_category";
        case Step::ForSaleCategory: return "akira/cloud/fetch_step_for_sale_category";
        case Step::StreamingAccess: return "akira/cloud/fetch_step_streaming_access";
        case Step::Matching: return "akira/cloud/fetch_step_matching";
        case Step::TrustedTime: return "akira/cloud/fetch_step_trusted_time";
        case Step::Exclusions: return "akira/cloud/fetch_step_exclusions";
        case Step::Idle:
        case Step::Ready:
        case Step::Unanswered:
        case Step::Cancelled:
            return std::nullopt;
    }
    return std::nullopt;
}

auto FetchProgress::keyOf(Activity activity) -> std::optional<std::string_view>
{
    switch (activity)
    {
        case Activity::ReusingHeldAnswer: return "akira/cloud/fetch_activity_reusing_held_answer";
        case Activity::WalkingPublicStore: return "akira/cloud/fetch_activity_walking_public_store";
        case Activity::WalkingOldServiceShelves: return "akira/cloud/fetch_activity_walking_old_service_shelves";
        case Activity::WalkingPublicStoreFallback: return "akira/cloud/fetch_activity_walking_public_store_fallback";
        case Activity::CheckingStreamingCopies: return "akira/cloud/fetch_activity_checking_streaming_copies";
        case Activity::ReadingPs5StoreLists: return "akira/cloud/fetch_activity_reading_ps5_store_lists";
        case Activity::ReadingEntitlements: return "akira/cloud/fetch_activity_reading_entitlements";
        case Activity::ReadingStreamedPs4Concepts: return "akira/cloud/fetch_activity_reading_streamed_ps4_concepts";
        case Activity::ScanningPs5Library: return "akira/cloud/fetch_activity_scanning_ps5_library";
        case Activity::ReadingCategoryPages: return "akira/cloud/fetch_activity_reading_category_pages";
        case Activity::ReadingSubscriptions: return "akira/cloud/fetch_activity_reading_subscriptions";
        case Activity::MatchingGames: return "akira/cloud/fetch_activity_matching_games";
        case Activity::SyncingClock: return "akira/cloud/fetch_activity_syncing_clock";
        case Activity::ReadingExclusions: return "akira/cloud/fetch_activity_reading_exclusions";
    }
    return std::nullopt;
}

void FollowedFetch::follow(uint64_t operation)
{
    followed = operation;
    current.reset();
}

auto FollowedFetch::observe(uint64_t operation, FetchProgress progress) -> Change
{
    if (operation != followed)
        return Change::Dropped;
    if (current == progress)
        return Change::Unchanged;
    const bool stepMoved = !current || current->shown() != progress.shown() || current->stepKey() != progress.stepKey();
    current = std::move(progress);
    return stepMoved ? Change::Step : Change::Lines;
}

} // namespace cloud
