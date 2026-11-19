#include "test_util.hpp"

#include "play/menu_hold.hpp"
#include "play/paused_frame.hpp"
#include "play/session_ending.hpp"
#include "play/stream_recovery.hpp"
#include "play/stream_route.hpp"
#include "play/stream_summary.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

using akira::play::MenuHold;
using akira::play::PausedFrame;
using akira::play::SessionEnding;
using akira::play::StreamProfile;
using akira::play::StreamRecovery;
using akira::play::StreamRoute;
using akira::play::StreamSummary;
using libchiaking::play::ConsoleQuit;
using libchiaking::play::EndReason;
using libchiaking::play::Outcome;

namespace {

Outcome quitWith(ConsoleQuit::Reason reason)
{
    Outcome::Evidence evidence;
    evidence.quit = ConsoleQuit(reason);
    return Outcome(evidence);
}

}

TEST(session_ending_reconnects_after_a_first_login_pin)
{
    SessionEnding ending(Outcome::not_started(EndReason::Cause::control_failed),
                         SessionEnding::Situation{.loginPinEntered = true});
    CHECK(ending.verdict() == SessionEnding::Verdict::Reconnect);

    SessionEnding stopped(quitWith(ConsoleQuit::Reason::stopped), SessionEnding::Situation{.loginPinEntered = true});
    CHECK(stopped.verdict() == SessionEnding::Verdict::Stopped);

    SessionEnding unpaired(Outcome::not_started(EndReason::Cause::unpaired_console),
                           SessionEnding::Situation{.loginPinEntered = true});
    CHECK(unpaired.verdict() == SessionEnding::Verdict::Unpaired);
}

TEST(session_ending_sends_cloud_failures_to_the_cloud_screen)
{
    SessionEnding cloud(quitWith(ConsoleQuit::Reason::trial_ended), SessionEnding::Situation{.cloudScreen = true});
    CHECK(cloud.verdict() == SessionEnding::Verdict::CloudFailure);
    CHECK(cloud.endsPlayTime());

    SessionEnding console(quitWith(ConsoleQuit::Reason::console_disconnected), SessionEnding::Situation{});
    CHECK(console.verdict() == SessionEnding::Verdict::Failed);
    CHECK(!console.endsPlayTime());
}

TEST(session_ending_names_the_reason)
{
    SessionEnding quit(quitWith(ConsoleQuit::Reason::console_in_use), SessionEnding::Situation{.duringConnect = true});
    CHECK_EQ(std::string(quit.reasonKey()), std::string("akira/stream/quit_rp_in_use"));
    CHECK_EQ(std::string(quit.failureKey()), std::string("akira/connection/fail_in_use"));
    CHECK(quit.duringConnect());

    SessionEnding cause(Outcome::not_started(EndReason::Cause::session_request_timeout), SessionEnding::Situation{});
    CHECK_EQ(std::string(cause.reasonKey()), std::string("akira/connection/fail_unreachable"));
}

TEST(session_ending_reads_an_awake_refusal_as_remote_play_off)
{
    SessionEnding awake(quitWith(ConsoleQuit::Reason::session_request_unknown),
                        SessionEnding::Situation{.consoleAwake = true});
    CHECK_EQ(std::string(awake.connectFailureKey()), std::string("akira/connection/fail_rp_disabled"));

    SessionEnding asleep(quitWith(ConsoleQuit::Reason::session_request_unknown), SessionEnding::Situation{});
    CHECK_EQ(std::string(asleep.connectFailureKey()), std::string("akira/connection/fail_generic"));
}

TEST(stream_route_picks_profile_and_context)
{
    CHECK(StreamRoute(std::string("Astro Bot"), true, true, true).profile() == StreamProfile::Cloud);
    CHECK_EQ(StreamRoute(std::string("Astro Bot"), false, false, true).context(), std::string("Cloud \xc2\xb7 Astro Bot"));
    CHECK(StreamRoute(std::nullopt, true, true, false).profile() == StreamProfile::Remote);
    CHECK_EQ(StreamRoute(std::nullopt, true, false, false).context(), std::string("PS4 \xc2\xb7 Remote"));
    CHECK(StreamRoute(std::nullopt, false, true, true).profile() == StreamProfile::Vpn);
    CHECK_EQ(StreamRoute(std::nullopt, false, true, true).context(), std::string("PS5 \xc2\xb7 VPN"));
    CHECK(StreamRoute(std::nullopt, false, false, true).profile() == StreamProfile::Local);
    CHECK_EQ(StreamRoute(std::nullopt, false, false, true).context(), std::string("PS5 \xc2\xb7 Direct"));
}

TEST(menu_hold_opens_after_three_seconds)
{
    MenuHold hold;
    const auto start = MenuHold::Clock::time_point{} + std::chrono::hours(1);
    CHECK(!hold.update(true, start));
    CHECK(hold.held());
    CHECK(!hold.update(true, start + std::chrono::milliseconds(2999)));
    CHECK_EQ(hold.heldFor(start + std::chrono::milliseconds(2999)).count(), 2);
    CHECK(hold.update(true, start + std::chrono::seconds(3)));
    CHECK(!hold.held());
    CHECK(!hold.update(true, start + std::chrono::seconds(4)));
    CHECK(!hold.update(false, start + std::chrono::seconds(8)));
    CHECK(!hold.update(true, start + std::chrono::seconds(9)));
    CHECK_EQ(hold.heldFor(start + std::chrono::seconds(11)).count(), 2);
}

TEST(stream_recovery_login_pin)
{
    using Step = StreamRecovery::LoginPinStep;
    CHECK(StreamRecovery::loginPin(true, false, "1234", true) == Step::Ignore);
    CHECK(StreamRecovery::loginPin(false, false, "1234", true) == Step::SubmitSaved);
    CHECK(StreamRecovery::loginPin(false, true, "1234", true) == Step::Ask);
    CHECK(StreamRecovery::loginPin(false, false, "", true) == Step::Ask);
    CHECK(StreamRecovery::loginPin(false, false, "1234", false) == Step::Ask);
}

TEST(stream_recovery_focus_prompts_only_for_a_dead_console_session)
{
    CHECK(StreamRecovery::watchesFocus(false, true, true, false));
    CHECK(!StreamRecovery::watchesFocus(true, true, true, false));
    CHECK(!StreamRecovery::watchesFocus(false, false, true, false));
    CHECK(!StreamRecovery::watchesFocus(false, true, false, false));
    CHECK(!StreamRecovery::watchesFocus(false, true, true, true));

    using libchiaking::play::SessionState;
    CHECK(StreamRecovery::sessionAlive(SessionState::playing));
    CHECK(StreamRecovery::sessionAlive(SessionState::connecting));
    CHECK(!StreamRecovery::sessionAlive(std::nullopt));
    CHECK(StreamRecovery::lost(false, true));
    CHECK(StreamRecovery::lost(true, false));
    CHECK(!StreamRecovery::lost(true, true));
}

TEST(stream_summary_grades_loss_and_formats_meta)
{
    akira::play::StreamStats stats;
    stats.packet_loss_percent = 0.4f;
    stats.requested_height = 720;
    stats.requested_fps = 60;
    stats.stream_duration_seconds = 185;
    CHECK(StreamSummary(stats).health() == StreamSummary::Health::Good);
    CHECK_EQ(StreamSummary(stats).meta(), std::string("720p60 \xc2\xb7 3m05s"));

    stats.packet_loss_percent = 0.5f;
    stats.video_height = 1080;
    CHECK(StreamSummary(stats).health() == StreamSummary::Health::Fair);
    CHECK_EQ(StreamSummary(stats).meta(), std::string("1080p60 \xc2\xb7 3m05s"));

    stats.packet_loss_percent = 2.0f;
    CHECK(StreamSummary(stats).health() == StreamSummary::Health::Poor);
}

TEST(paused_frame_blurs_and_drops_pixels)
{
    std::vector<std::uint8_t> pixels(16 * 16 * 4, 0);
    pixels[(8 * 16 + 8) * 4] = 255;
    PausedFrame frame(pixels, 16, 16);
    REQUIRE(frame.ready());
    CHECK_EQ(frame.width(), 16);
    CHECK_EQ(frame.height(), 16);
    CHECK(frame.pixels()[(8 * 16 + 8) * 4] < 255);
    CHECK(frame.pixels()[(8 * 16 + 9) * 4] > 0);
    frame.dropPixels();
    CHECK(!frame.ready());
    CHECK(!PausedFrame{}.ready());

    std::vector<std::uint8_t> flat(4 * 4 * 4, 77);
    PausedFrame even(flat, 4, 4);
    CHECK(even.pixels() == flat);
}
