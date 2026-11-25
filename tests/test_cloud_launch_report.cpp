#include "test_util.hpp"

#include <chrono>

#include "cloud/launch_report.hpp"
#include "fake_strings.hpp"

namespace {

using akira::cloud::LaunchReport;
using libchiaking::play::LaunchActivity;
using libchiaking::play::LaunchProgress;
using libchiaking::progress::Wait;
using libchiaking::progress::WaitReason;
using namespace std::chrono_literals;

constexpr auto ps5 = libchiaking::catalog::ServiceGeneration::ps5_cloud;

auto strings() -> tests::FakeStrings
{
    return tests::FakeStrings({
        { "akira/cloud/start_checking", "Checking" },
        { "akira/cloud/start_preparing", "Preparing" },
        { "akira/cloud/start_finding_server", "Finding a server" },
        { "akira/cloud/start_connecting", "Connecting" },
        { "akira/cloud/start_queued", "Queued at {0}" },
        { "akira/cloud/activity_allocating", "Allocating a server" },
        { "akira/cloud/activity_bandwidth", "Measuring your connection" },
        { "akira/cloud/detail_step", "Step {0} of {1}" },
        { "akira/cloud/detail_queue", "Position {0}" },
        { "akira/cloud/detail_rate", "{0} Mbps" },
        { "akira/cloud/detail_rate_saved", "{0} saved {1} min ago" },
        { "akira/cloud/wait_estimate_minutes", "about {0} mins" },
        { "akira/cloud/wait_server_queue", "In the queue" },
        { "akira/cloud/wait_trying_in", "{0}, trying again in {1} s" },
        { "akira/cloud/wait_trying_now", "{0}, trying again now" },
        { "akira/cloud/wait_settling", "Waiting {0} seconds" },
        { "akira/cloud/wait_settling_one", "Waiting {0} second" },
        { "akira/cloud/detail_tried", "Tried {0} times" },
        { "akira/cloud/detail_elapsed", "{0} s elapsed" },
    });
}

}

TEST(cloud_launch_report_starts_on_checking_with_no_lines)
{
    const tests::FakeStrings text = strings();
    LaunchReport report;
    CHECK(!report.reported());
    CHECK(report.step() == 1);
    CHECK(report.status(text) == "Checking");
    CHECK(report.activity(text).empty());
    CHECK(report.details(text, LaunchReport::Clock::now()).empty());
}

TEST(cloud_launch_report_spells_out_the_queue_wait)
{
    const tests::FakeStrings text = strings();
    const auto progress = LaunchProgress::at(ps5, LaunchActivity::allocating_server,
        LaunchProgress::Counts{ .step = LaunchProgress::Step{ .number = 5, .of = 7 },
            .queue_position = 4,
            .wait_time_estimate_seconds = 150,
            .attempt = 2,
            .wait = Wait{ .reason = WaitReason::server_queue, .next_try_in = 10'000ms },
            .elapsed = 42s });
    REQUIRE(progress.has_value());
    LaunchReport report;
    const auto now = LaunchReport::Clock::now();
    report.follow(*progress, now);
    CHECK(report.step() == 3);
    CHECK(report.status(text) == "Queued at 4");
    CHECK(report.activity(text) == "Allocating a server");
    CHECK(report.details(text, now + 3s)
        == "Step 5 of 7, Position 4, about 2 mins, In the queue, trying again in 7 s, Tried 2 times, 45 s elapsed");
    CHECK(report.details(text, now + 30s).find("In the queue, trying again now") != std::string::npos);
}

TEST(cloud_launch_report_never_steps_back)
{
    const tests::FakeStrings text = strings();
    LaunchReport report;
    const auto now = LaunchReport::Clock::now();
    report.follow(*LaunchProgress::at(ps5, LaunchActivity::connecting_to_server), now);
    CHECK(report.step() == 4);
    report.follow(*LaunchProgress::at(ps5, LaunchActivity::allocating_server), now);
    CHECK(report.step() == 4);
    CHECK(report.status(text) == "Connecting");
}

TEST(cloud_launch_report_ages_a_saved_bandwidth)
{
    const tests::FakeStrings text = strings();
    const auto progress = LaunchProgress::at(ps5, LaunchActivity::measuring_bandwidth,
        LaunchProgress::Counts{ .bandwidth_kbps = 25'000, .bandwidth_age = 180s });
    REQUIRE(progress.has_value());
    LaunchReport report;
    const auto now = LaunchReport::Clock::now();
    report.follow(*progress, now);
    CHECK(report.details(text, now) == "25 Mbps saved 3 min ago");
}
