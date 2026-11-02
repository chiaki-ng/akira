#include "cloud/launch_screen.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>

#include "ui/theme.hpp"

#include <algorithm>
#include <format>

using namespace brls::literals;

namespace cloud {

namespace play = libchiaking::play;
namespace progress = libchiaking::progress;

LaunchScreen::LaunchScreen(std::string name)
    : title(std::move(name))
{
}

void LaunchScreen::progress(const play::LaunchProgress& reported)
{
    latest = reported;
    received = Clock::now();
    steps.advance(stepOf(reported.phase()));
}

void LaunchScreen::playing()
{
    steps.playing();
}

void LaunchScreen::failed()
{
    steps.fail();
}

auto LaunchScreen::sinceReport() const -> int
{
    if (!latest)
        return 0;
    return static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - received).count());
}

auto LaunchScreen::stepOf(play::LaunchPhase phase) -> StartSteps::Step
{
    switch (phase)
    {
        case play::LaunchPhase::starting: return StartSteps::Step::Checking;
        case play::LaunchPhase::preparing: return StartSteps::Step::Preparing;
        case play::LaunchPhase::finding_server: return StartSteps::Step::FindingServer;
        case play::LaunchPhase::connecting: return StartSteps::Step::Connecting;
    }
    return StartSteps::Step::Checking;
}

auto LaunchScreen::stepLabel(StartSteps::Step step) -> std::string
{
    switch (step)
    {
        case StartSteps::Step::Checking: return "akira/cloud/start_checking"_i18n;
        case StartSteps::Step::Preparing: return "akira/cloud/start_preparing"_i18n;
        case StartSteps::Step::FindingServer: return "akira/cloud/start_finding_server"_i18n;
        case StartSteps::Step::Connecting: return "akira/cloud/start_connecting"_i18n;
    }
    return {};
}

auto LaunchScreen::status() const -> std::string
{
    if (latest && steps.current() <= StartSteps::Step::FindingServer)
        if (const auto queue = latest->queue_position())
            return brls::getStr("akira/cloud/start_queued", *queue);
    return stepLabel(steps.current());
}

auto LaunchScreen::activityLabel(play::LaunchActivity activity) -> std::string
{
    using A = play::LaunchActivity;
    switch (activity)
    {
        case A::clearing_with_catalog: return "akira/cloud/activity_clearing"_i18n;
        case A::checking_device: return "akira/cloud/activity_checking_device"_i18n;
        case A::opening_title_session: return "akira/cloud/activity_title_session"_i18n;
        case A::resolving_title: return "akira/cloud/activity_resolving_title"_i18n;
        case A::taking_free_licence: return "akira/cloud/activity_free_licence"_i18n;
        case A::opening_account_session: return "akira/cloud/activity_account_session"_i18n;
        case A::reserving_server: return "akira/cloud/activity_reserving"_i18n;
        case A::measuring_datacenters: return "akira/cloud/activity_measuring_datacenters"_i18n;
        case A::waiting_for_datacenter_choice: return "akira/cloud/activity_datacenter_choice"_i18n;
        case A::allocating_server: return "akira/cloud/activity_allocating"_i18n;
        case A::migrating_data: return "akira/cloud/activity_migrating"_i18n;
        case A::connecting_to_server: return "akira/cloud/activity_connecting"_i18n;
        case A::measuring_bandwidth: return "akira/cloud/activity_bandwidth"_i18n;
        case A::loading_library: return "akira/cloud/activity_library"_i18n;
        case A::checking_membership: return "akira/cloud/activity_membership"_i18n;
        case A::checking_cloud_streaming: return "akira/cloud/activity_premium"_i18n;
        case A::checking_ownership: return "akira/cloud/activity_ownership"_i18n;
        case A::checking_entitlement: return "akira/cloud/activity_entitlement"_i18n;
        case A::resolving_cloud_game: return "akira/cloud/activity_cloud_game"_i18n;
        case A::checking_exclusions: return "akira/cloud/activity_exclusions"_i18n;
        case A::checking_release: return "akira/cloud/activity_release"_i18n;
    }
    return {};
}

auto LaunchScreen::waitLabel(progress::WaitReason reason) -> std::string
{
    using R = progress::WaitReason;
    switch (reason)
    {
        case R::previous_session_closing: return "akira/cloud/wait_previous_session"_i18n;
        case R::server_queue: return "akira/cloud/wait_server_queue"_i18n;
        case R::data_migration: return "akira/cloud/wait_data_migration"_i18n;
        case R::datacenter_choice: return "akira/cloud/wait_datacenter_choice"_i18n;
        case R::title_not_ready: return "akira/cloud/wait_title_not_ready"_i18n;
        case R::network_unreachable: return "akira/cloud/wait_network"_i18n;
        case R::console_waking: return "akira/cloud/wait_console_waking"_i18n;
        case R::console_joining: return "akira/cloud/wait_console_joining"_i18n;
        case R::console_answering: return "akira/cloud/wait_console_answering"_i18n;
        case R::connection_retry: return "akira/cloud/wait_retry"_i18n;
        case R::recent_cloud_session: return "akira/cloud/wait_recent_session"_i18n;
    }
    return {};
}

auto LaunchScreen::waitText(const progress::Wait& wait, int since) -> std::string
{
    const std::string reason = waitLabel(wait.reason);
    if (!wait.next_try_in || wait.next_try_in->count() <= 0)
        return reason;
    const int64_t next = wait.next_try_in->count();
    const int64_t remaining = std::max<int64_t>(0, (next + 999) / 1000 - since);
    if (wait.reason == progress::WaitReason::recent_cloud_session)
        return brls::getStr(remaining == 1 ? "akira/cloud/wait_settling_one" : "akira/cloud/wait_settling", remaining);
    return remaining == 0 ? brls::getStr("akira/cloud/wait_trying_now", reason)
                          : brls::getStr("akira/cloud/wait_trying_in", reason, remaining);
}

auto LaunchScreen::waitEstimate(uint32_t seconds) -> std::string
{
    if (seconds < 60)
        return "akira/cloud/wait_estimate_short"_i18n;
    const uint32_t minutes = seconds / 60;
    return brls::getStr(minutes == 1 ? "akira/cloud/wait_estimate_minute" : "akira/cloud/wait_estimate_minutes",
                        minutes);
}

auto LaunchScreen::operationTitle() const -> std::optional<std::string>
{
    if (!latest || !latest->activity())
        return std::nullopt;
    return activityLabel(*latest->activity());
}

auto LaunchScreen::operationDetails(int since) const -> std::string
{
    if (!latest || !latest->activity())
        return {};
    std::vector<std::string> details;
    if (const auto step = latest->step())
        details.push_back(brls::getStr("akira/cloud/detail_step", step->number, step->of));
    if (const auto measured = latest->measured())
        details.push_back(brls::getStr("akira/cloud/detail_measured", measured->number, measured->of));
    if (const auto queue = latest->queue_position())
        details.push_back(brls::getStr("akira/cloud/detail_queue", *queue));
    if (const auto estimate = latest->wait_time_estimate_seconds())
        details.push_back(waitEstimate(*estimate));
    if (const auto migration = latest->migration_percent())
        details.push_back(brls::getStr("akira/cloud/detail_migration", *migration));
    if (const auto kbps = latest->bandwidth_kbps())
    {
        const std::string rate = brls::getStr("akira/cloud/detail_rate", *kbps / 1000);
        if (const auto age = latest->bandwidth_age())
            details.push_back(age->count() < 60
                ? brls::getStr("akira/cloud/detail_rate_recent", rate)
                : brls::getStr("akira/cloud/detail_rate_saved", rate, age->count() / 60));
        else
            details.push_back(rate);
    }
    if (const auto wait = latest->wait())
        details.push_back(waitText(*wait, since));
    if (latest->wait() && latest->attempt() && *latest->attempt() > 0)
        details.push_back(*latest->attempt() == 1 ? "akira/cloud/detail_tried_once"_i18n
                                                  : brls::getStr("akira/cloud/detail_tried", *latest->attempt()));
    if (const auto elapsed = latest->elapsed())
        details.push_back(brls::getStr("akira/cloud/detail_elapsed",
            std::chrono::duration_cast<std::chrono::seconds>(*elapsed).count() + since));

    std::string joined;
    for (const std::string& detail : details)
        joined += (joined.empty() ? "" : ", ") + detail;
    return joined;
}

void LaunchScreen::draw(NVGcontext* vg, float x, float y, float width, float height) const
{
    const auto& pal = akira::ui::active();
    nvgSave(vg);

    nvgBeginPath(vg);
    nvgRect(vg, x, y, width, height);
    nvgFillColor(vg, pal.backgroundDeep);
    nvgFill(vg);

    const float cx = x + width / 2.0f;
    float cursor = y + height * 0.28f;

    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFontSize(vg, 30.0f);
    nvgFillColor(vg, pal.text);
    const std::string heading = "akira/cloud/start_title"_i18n;
    nvgText(vg, cx, cursor, heading.c_str(), nullptr);

    cursor += 36.0f;
    nvgFontSize(vg, 19.0f);
    nvgFillColor(vg, pal.textMuted);
    nvgText(vg, cx, cursor, title.c_str(), nullptr);

    cursor += 54.0f;
    constexpr float slot = 170.0f;
    const float left = cx - slot * 2.0f;
    for (std::size_t i = 0; i < std::size(StartSteps::all); ++i)
    {
        const StartSteps::Step step = StartSteps::all[i];
        const StartSteps::Mark mark = steps.mark(step);
        const float sx = left + slot * static_cast<float>(i) + slot / 2.0f;
        const float barWidth = mark == StartSteps::Mark::Running ? 120.0f : 96.0f;

        NVGcolor fill = akira::ui::withAlpha(pal.textMuted, 0x48);
        if (mark == StartSteps::Mark::Done)
            fill = pal.success;
        else if (mark == StartSteps::Mark::Running)
            fill = pal.accent;
        else if (mark == StartSteps::Mark::Failed)
            fill = pal.danger;

        nvgBeginPath(vg);
        nvgRoundedRect(vg, sx - barWidth / 2.0f, cursor - 3.0f, barWidth, 6.0f, 3.0f);
        nvgFillColor(vg, fill);
        nvgFill(vg);

        nvgFontSize(vg, 15.0f);
        nvgFillColor(vg, mark == StartSteps::Mark::Pending ? pal.textDim : pal.text);
        const std::string label = stepLabel(step);
        nvgText(vg, sx, cursor + 22.0f, label.c_str(), nullptr);
    }

    cursor += 72.0f;
    nvgFontSize(vg, 18.0f);
    nvgFillColor(vg, pal.textMuted);
    const std::string line = status();
    nvgText(vg, cx, cursor, line.c_str(), nullptr);

    if (const auto operation = operationTitle())
    {
        cursor += 38.0f;
        nvgFontSize(vg, 16.0f);
        nvgFillColor(vg, pal.text);
        nvgText(vg, cx, cursor, operation->c_str(), nullptr);

        const std::string details = operationDetails(sinceReport());
        if (!details.empty())
        {
            cursor += 16.0f;
            nvgFontSize(vg, 15.0f);
            nvgFillColor(vg, pal.textDim);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
            nvgTextBox(vg, cx - 360.0f, cursor, 720.0f, details.c_str(), nullptr);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        }
    }

    nvgFontSize(vg, 16.0f);
    nvgFillColor(vg, pal.textDim);
    const std::string hint = steps.failed() ? std::string() : "akira/cloud/start_cancel_hint"_i18n;
    nvgText(vg, cx, y + height - 48.0f, hint.c_str(), nullptr);

    nvgRestore(vg);
}

} // namespace cloud
