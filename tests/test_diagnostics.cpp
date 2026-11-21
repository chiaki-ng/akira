#include "test_util.hpp"

#include <chrono>
#include <filesystem>
#include <format>

#include "play/diagnostics_history.hpp"
#include "play/diagnostics_report.hpp"
#include "play/stream_settings.hpp"
#include "settings/settings_store.hpp"

using akira::play::DiagnosticsAppearance;
using akira::play::DiagnosticsHistory;
using akira::play::DiagnosticsReport;
using akira::play::StreamStats;

namespace {

auto scratchFolder() -> std::filesystem::path
{
    return std::filesystem::temp_directory_path()
        / std::format("akira-diagnostics-{}", std::chrono::steady_clock::now().time_since_epoch().count());
}

auto steady() -> StreamStats
{
    StreamStats stats;
    stats.requested_fps = 60;
    stats.requested_height = 1080;
    stats.source_fps = 60.0f;
    stats.measured_bitrate_mbps = 20.0f;
    stats.visual_ms = 6.0f;
    stats.rtt_valid = true;
    stats.rtt_ms = 12.0f;
    return stats;
}

}

TEST(diagnostics_history_counts_frames_lost_per_interval)
{
    DiagnosticsHistory history;
    StreamStats stats = steady();
    stats.network_frames_lost = 100;
    history.record(stats, 0.5);
    stats.network_frames_lost = 103;
    history.record(stats, 0.5);
    CHECK_EQ(history.recent(&DiagnosticsHistory::Interval::framesLost), std::uint64_t{ 3 });
    const std::vector<std::size_t> marks = history.marks(DiagnosticsHistory::Event::NetworkLoss);
    REQUIRE(marks.size() == 1);
    CHECK_EQ(marks.front(), DiagnosticsHistory::capacity - 1);
}

TEST(diagnostics_history_keeps_its_capacity_and_window)
{
    DiagnosticsHistory history;
    StreamStats stats = steady();
    for (std::size_t step = 0; step < DiagnosticsHistory::capacity + 30; ++step)
    {
        stats.network_frames_lost += 1;
        history.record(stats, 0.5);
    }
    CHECK_EQ(history.series(&DiagnosticsHistory::Frame::bitrateMbps).size(), DiagnosticsHistory::capacity);
    CHECK_EQ(history.recent(&DiagnosticsHistory::Interval::framesLost), std::uint64_t{ DiagnosticsHistory::window });
}

TEST(diagnostics_report_grades_assembly_by_recent_losses)
{
    DiagnosticsHistory history;
    StreamStats stats = steady();
    history.record(stats, 0.5);
    stats.network_frames_lost = 12;
    history.record(stats, 0.5);
    const DiagnosticsReport report(DiagnosticsReport::Sample{ .stats = stats }, history);
    const std::vector<DiagnosticsReport::Stage> stages = report.stages();
    REQUIRE(stages.size() == 5);
    CHECK(stages[1].kind == DiagnosticsReport::StageKind::Assembly);
    CHECK_EQ(stages[1].headline, std::string("12"));
    CHECK(stages[1].tone == DiagnosticsReport::Tone::Bad);
    CHECK(report.overallTone() == DiagnosticsReport::Tone::Bad);
    const std::optional<DiagnosticsReport::Alert> alert = report.compactAlert();
    REQUIRE(alert.has_value());
    CHECK(alert->kind == DiagnosticsReport::StageKind::Assembly);
}

TEST(diagnostics_report_is_quiet_on_a_healthy_stream)
{
    DiagnosticsHistory history;
    history.record(steady(), 0.5);
    const DiagnosticsReport report(DiagnosticsReport::Sample{ .stats = steady() }, history);
    CHECK(report.compactTone() == DiagnosticsReport::Tone::Good);
    CHECK(!report.compactAlert().has_value());
    CHECK_EQ(report.rttValue(), std::string("12 ms"));
    CHECK_EQ(report.fullSections().size(), std::size_t{ 4 });
}

TEST(diagnostics_appearance_persists_in_the_overlay_table)
{
    const std::filesystem::path root = scratchFolder();
    {
        akira::settings::SettingsStore store(root);
        akira::play::StreamSettings stream(store, false);
        store.finishLoading();
        stream.setDiagnostics(DiagnosticsAppearance{ .level = DiagnosticsAppearance::Level::Standard,
            .glass = DiagnosticsAppearance::Glass::ClearDimmed,
            .layout = DiagnosticsAppearance::Layout::Floating });
        store.save();
    }
    {
        akira::settings::SettingsStore store(root);
        akira::play::StreamSettings stream(store, false);
        store.finishLoading();
        const DiagnosticsAppearance expected{ .level = DiagnosticsAppearance::Level::Standard,
            .glass = DiagnosticsAppearance::Glass::ClearDimmed,
            .layout = DiagnosticsAppearance::Layout::Floating };
        CHECK(stream.diagnostics() == expected);
    }
    std::filesystem::remove_all(root);
}
