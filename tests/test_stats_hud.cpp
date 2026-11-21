#include "test_util.hpp"

#include "play/stats_hud_placement.hpp"
#include "play/stats_readout.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using akira::play::StatsHudPlacement;
using akira::play::StatsReadout;
using akira::play::StreamStats;
using akira::play::TouchClaim;
using akira::play::UpscalingSummary;

namespace {

auto healthyStats() -> StreamStats
{
    StreamStats stats;
    stats.requested_width = 1920;
    stats.requested_height = 1080;
    stats.requested_fps = 60;
    stats.requested_bitrate = 30000;
    stats.requested_hevc = true;
    stats.is_hardware_decoder = true;
    stats.is_hevc = true;
    stats.fps = 59.9f;
    stats.video_width = 1920;
    stats.video_height = 1080;
    stats.measured_bitrate_mbps = 24.6f;
    stats.packet_loss_percent = 0.2f;
    stats.network_frames_lost = 0;
    stats.frames_recovered = 4;
    stats.stream_duration_seconds = 754;
    stats.rtt_valid = true;
    stats.rtt_ms = 12.0f;
    stats.decode_ms = 4.2f;
    stats.jitter_ms = 1.2f;
    stats.source_fps = 60.0f;
    stats.visual_ms = 8.4f;
    stats.net_ms = 7.2f;
    stats.total_ms = 15.6f;
    stats.latency_valid = true;
    return stats;
}

auto near(float left, float right) -> bool
{
    return std::fabs(left - right) < 0.01f;
}

auto values(const std::vector<StatsReadout::Cell>& cells) -> std::vector<std::string>
{
    std::vector<std::string> shown;
    for (const StatsReadout::Cell& cell : cells)
        shown.push_back(cell.value);
    return shown;
}

}

TEST(stats_readout_formats_the_compact_line_like_the_overlay)
{
    const StatsReadout readout(StatsReadout::Sample{ .stats = healthyStats(), .decoder = "NVTEGRA" });
    CHECK(values(readout.compact()) == std::vector<std::string>({ "59.9", "24.6", "0.2", "12", "16", "12m34s" }));
    CHECK(readout.overall() == StatsReadout::Tone::Good);
    CHECK_EQ(readout.context(), std::string("PS5 \xc2\xb7 Direct"));
    CHECK(values(readout.requested()) == std::vector<std::string>({ "1920x1080", "60", "30.0", "HEVC" }));
    CHECK(values(readout.output()) == std::vector<std::string>({ "1920x1080", "59.9", "NVTEGRA", "HEVC" }));
    CHECK(values(readout.footer()) == std::vector<std::string>({ "60.0", "PMULL", "Off", "4" }));
}

TEST(stats_readout_grades_with_the_overlay_thresholds)
{
    StreamStats stats = healthyStats();
    stats.fps = 50.0f;
    stats.packet_loss_percent = 2.5f;
    stats.rtt_ms = 60.0f;
    stats.network_frames_lost = 21;
    stats.total_ms = 120.0f;
    stats.measured_bitrate_mbps = 15.0f;
    const StatsReadout readout(StatsReadout::Sample{ .stats = stats });
    const std::vector<StatsReadout::Cell> line = readout.compact();
    CHECK(line[0].tone == StatsReadout::Tone::Warn);
    CHECK(line[1].tone == StatsReadout::Tone::Warn);
    CHECK(line[2].tone == StatsReadout::Tone::Bad);
    CHECK(line[3].tone == StatsReadout::Tone::Warn);
    CHECK(line[4].tone == StatsReadout::Tone::Bad);
    CHECK(readout.link()[3].tone == StatsReadout::Tone::Bad);
    CHECK(readout.overall() == StatsReadout::Tone::Bad);
    CHECK(readout.bar().filled == 1.0f);
    CHECK(near(readout.bar().network, 0.06f));
}

TEST(stats_readout_hides_unknown_round_trips_and_shows_software_decode)
{
    StreamStats stats = healthyStats();
    stats.rtt_valid = false;
    stats.latency_valid = false;
    stats.is_hardware_decoder = false;
    const StatsReadout readout(
        StatsReadout::Sample{ .stats = stats, .decoder = "NVTEGRA", .tunnel = std::string("10.8.0.6") });
    CHECK_EQ(readout.compact()[3].value, std::string("--"));
    CHECK_EQ(readout.latency().value, std::string("--"));
    CHECK_EQ(readout.output()[2].value, std::string("SW"));
    CHECK_EQ(readout.context(), std::string("PS5 \xc2\xb7 VPN"));
    CHECK_EQ(readout.footer()[2].value, std::string("10.8.0.6"));
    CHECK(readout.bar().network == 0.0f);
}

TEST(stats_readout_describes_upscaling)
{
    const StatsReadout readout(StatsReadout::Sample{ .stats = healthyStats(),
        .upscaling = UpscalingSummary{ .active = true, .easu = true, .rcas = true, .sharpness = 0.2f, .inputHeight = 720,
            .targetHeight = 1080, .displayHeight = 1080 } });
    CHECK(readout.upscaled());
    CHECK_EQ(readout.upscalePath(), std::string("720\xe2\x80\xba" "1080\xe2\x80\xba" "1080"));
    CHECK_EQ(readout.upscaleDetail(), std::string("EASU \xc2\xb7 RCAS 90%"));
    CHECK_EQ(readout.upscaleSummary(), std::string("1.50\xc3\x97 EASU \xc2\xb7 RCAS 90%"));
    const StatsReadout sharpened(StatsReadout::Sample{ .stats = healthyStats(),
        .upscaling = UpscalingSummary{ .active = true, .rcas = true, .sharpness = 0.0f, .inputHeight = 1080,
            .displayHeight = 1080 } });
    CHECK_EQ(sharpened.upscaleSummary(), std::string("RCAS 100%"));
    CHECK_EQ(sharpened.upscalePath(), std::string("1080\xe2\x80\xba" "1080"));
}

TEST(stats_hud_placement_starts_at_the_margin_and_clamps_saved_positions)
{
    StatsHudPlacement placement;
    placement.measureScreen({ .width = 1280.0f, .height = 720.0f });
    placement.measurePanel({ .width = 400.0f, .height = 40.0f });
    CHECK((placement.origin() == StatsHudPlacement::Point{ .x = 24.0f, .y = 24.0f }));
    placement.restore(TouchClaim::Fraction{ .x = 0.9f, .y = 0.99f });
    CHECK((placement.origin() == StatsHudPlacement::Point{ .x = 856.0f, .y = 656.0f }));
}

TEST(stats_hud_placement_drags_only_a_grab_on_the_shown_panel)
{
    StatsHudPlacement placement;
    placement.measureScreen({ .width = 1280.0f, .height = 720.0f });
    placement.measurePanel({ .width = 400.0f, .height = 40.0f });
    CHECK(!placement.grab({ .x = 50.0f / 1280.0f, .y = 40.0f / 720.0f }));
    placement.show(true);
    CHECK(!placement.grab({ .x = 640.0f / 1280.0f, .y = 360.0f / 720.0f }));
    const std::uint64_t before = placement.revision();
    CHECK(placement.grab({ .x = 50.0f / 1280.0f, .y = 40.0f / 720.0f }));
    placement.drag({ .x = 326.0f / 1280.0f, .y = 216.0f / 720.0f });
    CHECK(placement.revision() != before);
    CHECK(near(placement.origin().x, 300.0f));
    CHECK(near(placement.origin().y, 200.0f));
    CHECK(!placement.takeSettled().has_value());
    placement.release();
    const auto settled = placement.takeSettled();
    REQUIRE(settled.has_value());
    CHECK(near(settled->x * 1280.0f, 300.0f));
    CHECK(near(settled->y * 720.0f, 200.0f));
    CHECK(!placement.takeSettled().has_value());
}

TEST(stats_hud_placement_settles_inside_the_margin)
{
    StatsHudPlacement placement;
    placement.measureScreen({ .width = 1280.0f, .height = 720.0f });
    placement.measurePanel({ .width = 400.0f, .height = 40.0f });
    placement.show(true);
    REQUIRE(placement.grab({ .x = 30.0f / 1280.0f, .y = 30.0f / 720.0f }));
    placement.drag({ .x = 0.0f, .y = 0.0f });
    placement.release();
    const auto settled = placement.takeSettled();
    REQUIRE(settled.has_value());
    CHECK(near(settled->x * 1280.0f, 24.0f));
    CHECK(near(settled->y * 720.0f, 24.0f));
}

TEST(stats_hud_placement_scales_a_large_panel_to_half_the_screen)
{
    StatsHudPlacement placement;
    placement.measureScreen({ .width = 1280.0f, .height = 720.0f });
    placement.measurePanel({ .width = 896.0f, .height = 560.0f });
    CHECK(near(placement.scale(), 640.0f / 896.0f));
    placement.restore(TouchClaim::Fraction{ .x = 0.99f, .y = 0.99f });
    CHECK(near(placement.origin().x, 1280.0f - 640.0f - 24.0f));
    CHECK(near(placement.origin().y, 720.0f - 400.0f - 24.0f));
}

TEST(stats_hud_placement_keeps_a_small_panel_at_full_size)
{
    StatsHudPlacement placement;
    placement.measureScreen({ .width = 1280.0f, .height = 720.0f });
    placement.measurePanel({ .width = 400.0f, .height = 40.0f });
    CHECK(near(placement.scale(), 1.0f));
}
