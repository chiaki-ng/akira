#include "cloud/launch_report.hpp"

#include <algorithm>
#include <ranges>
#include <string_view>
#include <vector>

namespace akira::cloud {

namespace play = libchiaking::play;
namespace progress = libchiaking::progress;

void LaunchReport::follow(const play::LaunchProgress& progress, Clock::time_point now)
{
    latest = progress;
    received = now;
    reached = std::max(reached, stepOf(progress.phase()));
}

auto LaunchReport::stepOf(play::LaunchPhase phase) -> int
{
    switch (phase)
    {
        case play::LaunchPhase::starting: return 1;
        case play::LaunchPhase::preparing: return 2;
        case play::LaunchPhase::finding_server: return 3;
        case play::LaunchPhase::connecting: return 4;
    }
    return 1;
}

auto LaunchReport::stepKey(int step) -> const char*
{
    switch (step)
    {
        case 2: return "akira/cloud/start_preparing";
        case 3: return "akira/cloud/start_finding_server";
        case 4: return "akira/cloud/start_connecting";
        default: break;
    }
    return "akira/cloud/start_checking";
}

auto LaunchReport::status(const app::Strings& strings) const -> std::string
{
    if (latest && reached <= 3)
        if (const auto queue = latest->queue_position())
            return strings.format("akira/cloud/start_queued", *queue);
    return strings.text(stepKey(reached));
}

auto LaunchReport::activityKey(play::LaunchActivity activity) -> const char*
{
    using A = play::LaunchActivity;
    switch (activity)
    {
        case A::clearing_with_catalog: return "akira/cloud/activity_clearing";
        case A::checking_device: return "akira/cloud/activity_checking_device";
        case A::opening_title_session: return "akira/cloud/activity_title_session";
        case A::resolving_title: return "akira/cloud/activity_resolving_title";
        case A::taking_free_licence: return "akira/cloud/activity_free_licence";
        case A::opening_account_session: return "akira/cloud/activity_account_session";
        case A::reserving_server: return "akira/cloud/activity_reserving";
        case A::measuring_datacenters: return "akira/cloud/activity_measuring_datacenters";
        case A::waiting_for_datacenter_choice: return "akira/cloud/activity_datacenter_choice";
        case A::allocating_server: return "akira/cloud/activity_allocating";
        case A::migrating_data: return "akira/cloud/activity_migrating";
        case A::connecting_to_server: return "akira/cloud/activity_connecting";
        case A::measuring_bandwidth: return "akira/cloud/activity_bandwidth";
        case A::loading_library: return "akira/cloud/activity_library";
        case A::checking_membership: return "akira/cloud/activity_membership";
        case A::checking_cloud_streaming: return "akira/cloud/activity_premium";
        case A::checking_ownership: return "akira/cloud/activity_ownership";
        case A::checking_entitlement: return "akira/cloud/activity_entitlement";
        case A::resolving_cloud_game: return "akira/cloud/activity_cloud_game";
        case A::checking_exclusions: return "akira/cloud/activity_exclusions";
        case A::checking_release: return "akira/cloud/activity_release";
    }
    return "akira/cloud/activity_clearing";
}

auto LaunchReport::activity(const app::Strings& strings) const -> std::string
{
    if (!latest || !latest->activity())
        return {};
    return strings.text(activityKey(*latest->activity()));
}

auto LaunchReport::waitKey(progress::WaitReason reason) -> const char*
{
    using R = progress::WaitReason;
    switch (reason)
    {
        case R::previous_session_closing: return "akira/cloud/wait_previous_session";
        case R::server_queue: return "akira/cloud/wait_server_queue";
        case R::data_migration: return "akira/cloud/wait_data_migration";
        case R::datacenter_choice: return "akira/cloud/wait_datacenter_choice";
        case R::title_not_ready: return "akira/cloud/wait_title_not_ready";
        case R::network_unreachable: return "akira/cloud/wait_network";
        case R::console_waking: return "akira/cloud/wait_console_waking";
        case R::console_joining: return "akira/cloud/wait_console_joining";
        case R::console_answering: return "akira/cloud/wait_console_answering";
        case R::connection_retry: return "akira/cloud/wait_retry";
        case R::recent_cloud_session: return "akira/cloud/wait_recent_session";
    }
    return "akira/cloud/wait_retry";
}

auto LaunchReport::waitText(const app::Strings& strings, const progress::Wait& wait, std::int64_t since) -> std::string
{
    const std::string reason = strings.text(waitKey(wait.reason));
    if (!wait.next_try_in || wait.next_try_in->count() <= 0)
        return reason;
    const std::int64_t remaining = std::max<std::int64_t>(0, (wait.next_try_in->count() + 999) / 1000 - since);
    if (wait.reason == progress::WaitReason::recent_cloud_session)
        return strings.counted("akira/cloud/wait_settling", remaining);
    return remaining == 0 ? strings.format("akira/cloud/wait_trying_now", reason)
                          : strings.format("akira/cloud/wait_trying_in", reason, remaining);
}

auto LaunchReport::waitEstimate(const app::Strings& strings, std::uint32_t seconds) -> std::string
{
    if (seconds < 60)
        return strings.text("akira/cloud/wait_estimate_short");
    const std::uint32_t minutes = seconds / 60;
    return strings.format(minutes == 1 ? "akira/cloud/wait_estimate_minute" : "akira/cloud/wait_estimate_minutes", minutes);
}

auto LaunchReport::rate(const app::Strings& strings, std::uint32_t kbps, std::optional<std::chrono::seconds> age)
    -> std::string
{
    const std::string measured = strings.format("akira/cloud/detail_rate", kbps / 1000);
    if (!age)
        return measured;
    return age->count() < 60 ? strings.format("akira/cloud/detail_rate_recent", measured)
                             : strings.format("akira/cloud/detail_rate_saved", measured, age->count() / 60);
}

auto LaunchReport::details(const app::Strings& strings, Clock::time_point now) const -> std::string
{
    if (!latest || !latest->activity())
        return {};
    const std::int64_t since = std::max<std::int64_t>(0,
        std::chrono::duration_cast<std::chrono::seconds>(now - received).count());
    std::vector<std::string> parts;
    if (const auto step = latest->step())
        parts.push_back(strings.format("akira/cloud/detail_step", step->number, step->of));
    if (const auto measured = latest->measured())
        parts.push_back(strings.format("akira/cloud/detail_measured", measured->number, measured->of));
    if (const auto queue = latest->queue_position())
        parts.push_back(strings.format("akira/cloud/detail_queue", *queue));
    if (const auto estimate = latest->wait_time_estimate_seconds())
        parts.push_back(waitEstimate(strings, *estimate));
    if (const auto migration = latest->migration_percent())
        parts.push_back(strings.format("akira/cloud/detail_migration", *migration));
    if (const auto kbps = latest->bandwidth_kbps())
        parts.push_back(rate(strings, *kbps, latest->bandwidth_age()));
    if (const auto wait = latest->wait())
        parts.push_back(waitText(strings, *wait, since));
    if (latest->wait() && latest->attempt() && *latest->attempt() > 0)
        parts.push_back(*latest->attempt() == 1 ? strings.text("akira/cloud/detail_tried_once")
                                                : strings.format("akira/cloud/detail_tried", *latest->attempt()));
    if (const auto elapsed = latest->elapsed())
        parts.push_back(strings.format("akira/cloud/detail_elapsed",
            std::chrono::duration_cast<std::chrono::seconds>(*elapsed).count() + since));
    return parts | std::views::join_with(std::string_view(", ")) | std::ranges::to<std::string>();
}

}
