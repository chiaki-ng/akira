#include "test_util.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <thread>
#include <vector>

#include "play/audio_gain.hpp"
#include "play/present_loop.hpp"
#include "play/present_timing.hpp"

namespace {

using akira::play::PresentLoop;
using akira::play::PresentTiming;
using namespace std::chrono_literals;

class CountingTarget final : public PresentLoop::Target {
public:
    auto present(PresentTiming::Kind kind, PresentTiming& timing)
        -> std::expected<PresentLoop::Presented, PresentTiming::Miss> override
    {
        if (busy.load())
            return std::unexpected(PresentTiming::Miss::busy);
        (kind == PresentTiming::Kind::frame ? frames : repaints).fetch_add(1);
        if (copyEach > 0)
            timing.copied(copyEach);
        if (inside)
            inside();
        const auto now = PresentLoop::Clock::now();
        return PresentLoop::Presented{ .submitted = now, .presented = now };
    }

    void swapInterval(std::uint32_t interval) override { intervals.push_back(interval); }

    std::atomic<int> frames{ 0 };
    std::atomic<int> repaints{ 0 };
    std::atomic<bool> busy{ false };
    std::size_t copyEach = 0;
    std::function<void()> inside;
    std::vector<std::uint32_t> intervals;
};

auto waitFor(const std::function<bool()>& done) -> bool
{
    for (int attempt = 0; attempt < 200; ++attempt)
    {
        if (done())
            return true;
        std::this_thread::sleep_for(5ms);
    }
    return done();
}

}

TEST(present_timing_reports_once_per_window_with_interval_and_latency)
{
    PresentTiming timing;
    const auto start = PresentTiming::Clock::now();
    for (int frame = 0; frame < 61; ++frame)
    {
        const auto ready = start + frame * 16ms;
        timing.presented(PresentTiming::Kind::frame,
            PresentTiming::Stamps{ .ready = ready, .submitted = ready + 1ms, .presented = ready + 1ms + 50us });
        if (frame < 60)
            CHECK(!timing.report(ready + 1ms + 50us).has_value());
    }
    const std::optional<std::string> line = timing.report(start + 1002ms);
    REQUIRE(line.has_value());
    CHECK(line->find("[PRESENT] 61 frames + 0 repaints") == 0);
    CHECK(line->find("interval avg 16.00 p95 16.00 max 16.00 ms") != std::string::npos);
    CHECK(line->find("ready->submit avg 1.00 max 1.00 ms") != std::string::npos);
    CHECK(line->find("submit->present avg 0.050 max 0.050 ms") != std::string::npos);
    CHECK(line->find("copies 0 (0 B)") != std::string::npos);
    CHECK(!timing.report(start + 1003ms).has_value());
}

TEST(present_timing_counts_misses_and_copies_in_totals)
{
    PresentTiming timing;
    timing.missed(PresentTiming::Miss::busy);
    timing.missed(PresentTiming::Miss::unavailable);
    timing.missed(PresentTiming::Miss::unavailable);
    timing.copied(4096);
    const PresentTiming::Totals totals = timing.totals();
    CHECK_EQ(totals.busy, 1u);
    CHECK_EQ(totals.unavailable, 2u);
    CHECK_EQ(totals.copies, 1u);
    CHECK_EQ(totals.copiedBytes, 4096u);
}

TEST(present_loop_presents_each_ready_frame_at_the_live_interval)
{
    CountingTarget target;
    {
        PresentLoop loop(target, PresentLoop::Pacing{ .stallRepaint = 10s });
        for (int frame = 0; frame < 5; ++frame)
            loop.frameReady();
        CHECK_EQ(loop.totals().frames, 5u);
        CHECK_EQ(loop.totals().copies, 0u);
    }
    CHECK_EQ(target.frames.load(), 5);
    REQUIRE(target.intervals.size() == 1);
    CHECK_EQ(target.intervals.front(), 0u);
}

TEST(present_loop_repaints_while_paused_and_switches_the_interval)
{
    CountingTarget target;
    PresentLoop loop(target, PresentLoop::Pacing{ .pausedRepaint = 5ms, .stallRepaint = 10s });
    loop.frameReady();
    loop.paused(true);
    CHECK(waitFor([&] { return target.repaints.load() >= 3; }));
    loop.paused(false);
    loop.frameReady();
    loop.stop();
    REQUIRE(target.intervals.size() >= 2);
    CHECK_EQ(target.intervals[0], 0u);
    CHECK_EQ(target.intervals[1], 1u);
    CHECK_EQ(target.intervals.back(), 0u);
}

TEST(present_loop_does_not_repaint_before_the_first_frame)
{
    CountingTarget target;
    PresentLoop loop(target, PresentLoop::Pacing{ .pausedRepaint = 2ms, .stallRepaint = 2ms });
    loop.paused(true);
    std::this_thread::sleep_for(30ms);
    loop.stop();
    CHECK_EQ(target.repaints.load(), 0);
}

TEST(present_loop_counts_a_busy_slot_as_a_skip_without_waiting)
{
    CountingTarget target;
    PresentLoop loop(target, PresentLoop::Pacing{ .stallRepaint = 10s });
    target.busy.store(true);
    const auto before = PresentLoop::Clock::now();
    loop.frameReady();
    CHECK(PresentLoop::Clock::now() - before < 50ms);
    CHECK_EQ(loop.totals().busy, 1u);
    CHECK_EQ(loop.totals().frames, 0u);
}

TEST(present_loop_runs_serialized_work_inline_from_inside_a_present)
{
    CountingTarget target;
    PresentLoop loop(target, PresentLoop::Pacing{ .stallRepaint = 10s });
    int ran = 0;
    target.inside = [&] { loop.serialized([&] { ++ran; }); };
    loop.frameReady();
    target.inside = nullptr;
    loop.serialized([&] { ++ran; });
    CHECK_EQ(ran, 2);
}

TEST(present_loop_counts_a_copy_reported_by_the_target)
{
    CountingTarget target;
    target.copyEach = 1382400;
    PresentLoop loop(target, PresentLoop::Pacing{ .stallRepaint = 10s });
    loop.frameReady();
    CHECK_EQ(loop.totals().copies, 1u);
    CHECK_EQ(loop.totals().copiedBytes, 1382400u);
}

TEST(audio_gain_boosts_and_clips_like_the_sdl_path)
{
    const std::array<std::int16_t, 5> samples{ 0, 1000, -1000, 30000, -30000 };
    std::array<std::int16_t, 5> boosted{};
    CHECK_EQ(akira::play::AudioGain::apply(samples, boosted), 5u);
    CHECK_EQ(boosted[0], 0);
    CHECK_EQ(boosted[1], 1800);
    CHECK_EQ(boosted[2], -1800);
    CHECK_EQ(boosted[3], std::numeric_limits<std::int16_t>::max());
    CHECK_EQ(boosted[4], std::numeric_limits<std::int16_t>::min());
}

TEST(audio_gain_writes_no_further_than_the_target)
{
    const std::array<std::int16_t, 4> samples{ 1, 2, 3, 4 };
    std::array<std::int16_t, 2> into{};
    CHECK_EQ(akira::play::AudioGain::apply(samples, into), 2u);
    CHECK_EQ(into[1], 3);
}
