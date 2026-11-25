#include "play/diagnostics_report.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <ranges>
#include <utility>

namespace akira::play {

DiagnosticsReport::DiagnosticsReport(Sample sample, DiagnosticsHistory history)
    : sample(std::move(sample))
    , history(std::move(history))
{
}

auto DiagnosticsReport::stages() const -> std::vector<Stage>
{
    return { networkStage(), assemblyStage(), decodeStage(), renderStage(), displayStage() };
}

auto DiagnosticsReport::worst(const std::vector<Stage>& shown) -> Tone
{
    Tone found = Tone::Neutral;
    for (const Stage& stage : shown)
        found = std::max(found, stage.tone);
    return found;
}

auto DiagnosticsReport::overallTone() const -> Tone
{
    return worst(stages());
}

auto DiagnosticsReport::compactTone() const -> Tone
{
    std::vector<Stage> shown = stages();
    std::erase_if(shown, [](const Stage& stage) { return stage.kind == StageKind::Display; });
    return worst(shown);
}

auto DiagnosticsReport::compactAlert() const -> std::optional<Alert>
{
    std::vector<Stage> shown = stages();
    std::erase_if(shown, [](const Stage& stage) { return stage.kind == StageKind::Display; });
    const Tone tone = worst(shown);
    if (tone < Tone::Warning)
        return std::nullopt;
    const auto stage = std::ranges::find(shown, tone, &Stage::tone);
    if (const auto row = std::ranges::find(stage->rows, tone, &Row::tone); row != stage->rows.end())
        return Alert{ .kind = stage->kind, .label = row->label, .value = row->value, .tone = tone };
    if (stage->kind == StageKind::Network)
        return std::nullopt;
    return Alert{ .kind = stage->kind, .label = stage->unit, .value = stage->headline, .tone = tone, .headline = true };
}

auto DiagnosticsReport::headerChips() const -> std::vector<Chip>
{
    const StreamStats& stats = sample.stats;
    std::vector<Chip> chips;
    if (!sample.context.empty())
        chips.push_back(Chip{ .segments = { sample.context } });
    if (stats.takion_version)
        chips.push_back(Chip{ .segments = { std::format("Takion v{}", *stats.takion_version) } });
    std::string codec = stats.requested_hevc ? "HEVC" : "H.264";
    if (stats.video_width > 0)
        codec += " 8-bit";
    chips.push_back(Chip{ .segments = { codec } });
    if (stats.requested_height > 0)
        chips.push_back(Chip{ .segments = { stats.requested_fps > 0
                                      ? std::format("{}p{}", stats.requested_height, stats.requested_fps)
                                      : std::format("{}p", stats.requested_height) } });
    if (stats.console_quality_valid && stats.target_bitrate_kbps > 0)
    {
        const double offered = stats.target_bitrate_kbps / 1000.0;
        const double requested = stats.requested_bitrate / 1000.0;
        const bool shortfall = requested > 0.0 && offered < requested - 0.05;
        chips.push_back(Chip{ .segments = { std::format("{} / {} Mbps", oneDecimal(offered), oneDecimal(requested)) },
            .tone = shortfall ? Tone::Warning : Tone::Neutral });
    }
    if (const std::optional<std::string> scaling = upscaler())
        chips.push_back(Chip{ .segments = { *scaling } });
    if (!sample.decoder.empty())
        chips.push_back(Chip{ .segments = { sample.decoder } });
    return chips;
}

auto DiagnosticsReport::upscaler() const -> std::optional<std::string>
{
    const UpscalingSummary& scaling = sample.upscaling;
    if (!scaling.active)
        return std::nullopt;
    return std::format("FSR {}p\xe2\x86\x92{}p", scaling.inputHeight, scaling.targetHeight);
}

auto DiagnosticsReport::uptime() const -> std::string
{
    const std::uint64_t seconds = sample.stats.stream_duration_seconds;
    return std::format("{:02}:{:02}", seconds / 60, seconds % 60);
}

auto DiagnosticsReport::rttValue() const -> std::string
{
    const StreamStats& stats = sample.stats;
    if (!stats.rtt_valid)
        return unavailable;
    if (stats.rtt_variation_ms < 1.0f)
        return milliseconds(stats.rtt_ms);
    return std::format("{} \xc2\xb1 {} ms", millisecondsNumber(stats.rtt_ms), millisecondsNumber(stats.rtt_variation_ms));
}

auto DiagnosticsReport::lossValue() const -> std::string
{
    return std::format("{}%", oneDecimal(sample.stats.packet_loss_percent));
}

auto DiagnosticsReport::lossTone() const -> Tone
{
    return lossToneOf(sample.stats.packet_loss_percent);
}

auto DiagnosticsReport::bitrateValue() const -> std::string
{
    return oneDecimal(sample.stats.measured_bitrate_mbps);
}

auto DiagnosticsReport::latencySummary() const -> std::string
{
    const StreamStats& stats = sample.stats;
    return stats.latency_valid ? milliseconds(stats.total_ms) : std::string(unavailable);
}

auto DiagnosticsReport::events() const -> std::vector<DiagnosticsHistory::Event>
{
    std::vector<DiagnosticsHistory::Event> present;
    for (const DiagnosticsHistory::Event event : DiagnosticsHistory::events)
        if (!history.marks(event).empty())
            present.push_back(event);
    return present;
}

auto DiagnosticsReport::networkStage() const -> Stage
{
    const StreamStats& stats = sample.stats;
    std::vector<Row> rows;
    rows.push_back(Row{ .label = "akira/diagnostics/rtt", .value = rttValue() });
    if (stats.rtt_valid && stats.rtt_samples > 0)
    {
        rows.push_back(Row{ .label = "akira/diagnostics/rtt_spikes",
            .value = stats.rtt_recent_max_ms > 0.0f ? milliseconds(stats.rtt_recent_max_ms) : std::string(unavailable) });
        rows.push_back(Row{ .label = "akira/diagnostics/rtt_p99", .value = milliseconds(stats.rtt_p99_ms) });
    }
    rows.push_back(Row{ .label = "akira/diagnostics/loss", .value = lossValue(), .tone = lossTone() });
    return Stage{ .kind = StageKind::Network,
        .headline = bitrateValue(),
        .unit = "akira/diagnostics/unit_mbps",
        .tone = lossTone(),
        .series = history.series(&DiagnosticsHistory::Frame::bitrateMbps),
        .rows = std::move(rows) };
}

auto DiagnosticsReport::assemblyStage() const -> Stage
{
    const std::uint64_t lost = history.recent(&DiagnosticsHistory::Interval::framesLost);
    const std::uint64_t requests = history.recent(&DiagnosticsHistory::Interval::keyframeRequests);
    const std::uint64_t waits = history.recent(&DiagnosticsHistory::Interval::keyframeWaits);
    Tone tone = Tone::Good;
    if (lost >= 10)
        tone = Tone::Bad;
    else if (lost > 0)
        tone = Tone::Warning;
    return Stage{ .kind = StageKind::Assembly,
        .headline = std::to_string(lost),
        .unit = "akira/diagnostics/unit_lost_recent",
        .tone = tone,
        .series = history.series(&DiagnosticsHistory::Frame::framesLost),
        .rows = { Row{ .label = "akira/diagnostics/keyframes_asked", .value = std::to_string(requests), .tone = countTone(requests) },
            Row{ .label = "akira/diagnostics/keyframe_waits", .value = std::to_string(waits), .tone = countTone(waits) } } };
}

auto DiagnosticsReport::decodeStage() const -> Stage
{
    const StreamStats& stats = sample.stats;
    const std::uint64_t errors = history.recent(&DiagnosticsHistory::Interval::decodeErrors);
    const Tone source = history.empty() ? Tone::Neutral : ratioTone(stats.source_fps, std::max(1, stats.requested_fps));
    return Stage{ .kind = StageKind::Decode,
        .headline = oneDecimal(stats.source_fps),
        .unit = "akira/diagnostics/unit_fps",
        .tone = std::max(source, countTone(errors, true)),
        .series = history.series(&DiagnosticsHistory::Frame::decodedFps),
        .rows = { Row{ .label = "akira/diagnostics/errors", .value = std::to_string(errors), .tone = countTone(errors, true) },
            Row{ .label = "akira/diagnostics/time",
                .value = stats.decode_ms > 0.0f ? milliseconds(stats.decode_ms) : std::string(unavailable) } } };
}

auto DiagnosticsReport::renderStage() const -> Stage
{
    const StreamStats& stats = sample.stats;
    const double budget = budgetMilliseconds();
    Tone tone = Tone::Bad;
    if (stats.visual_ms < budget * 0.6)
        tone = Tone::Good;
    else if (stats.visual_ms < budget * 0.9)
        tone = Tone::Warning;
    return Stage{ .kind = StageKind::Render,
        .headline = millisecondsNumber(stats.visual_ms),
        .unit = "akira/diagnostics/unit_ms_frame",
        .tone = tone,
        .series = history.series(&DiagnosticsHistory::Frame::renderMilliseconds),
        .rows = { Row{ .label = "akira/diagnostics/budget", .value = milliseconds(budget) },
            Row{ .label = "akira/diagnostics/upscaler",
                .value = upscaler().value_or("akira/diagnostics/off"),
                .keyed = !upscaler().has_value() } } };
}

auto DiagnosticsReport::displayStage() const -> Stage
{
    const StreamStats& stats = sample.stats;
    return Stage{ .kind = StageKind::Display,
        .headline = stats.requested_fps > 0 ? std::to_string(stats.requested_fps) : std::string(unavailable),
        .unit = "akira/diagnostics/unit_hz",
        .tone = Tone::Neutral,
        .series = history.series(&DiagnosticsHistory::Frame::decodedFps),
        .rows = { Row{ .label = "akira/diagnostics/rendered", .value = std::format("{} fps", oneDecimal(stats.fps)) },
            Row{ .label = "akira/diagnostics/present", .value = milliseconds(stats.present_ms) },
            Row{ .label = "akira/diagnostics/jitter", .value = milliseconds(stats.jitter_ms) } } };
}

auto DiagnosticsReport::fullSections() const -> std::vector<Section>
{
    return { networkSection(), framesSection(), presentationSection(), latencySection() };
}

auto DiagnosticsReport::networkSection() const -> Section
{
    const StreamStats& stats = sample.stats;
    const double target = stats.console_quality_valid && stats.target_bitrate_kbps > 0 ? stats.target_bitrate_kbps / 1000.0
                                                                                      : stats.requested_bitrate / 1000.0;
    std::vector<Row> rows;
    rows.push_back(Row{ .label = "akira/diagnostics/bitrate",
        .value = target > 0.0 ? std::format("{} / {} Mbps", bitrateValue(), oneDecimal(target))
                              : std::format("{} Mbps", bitrateValue()) });
    if (stats.transfer_valid)
        rows.push_back(Row{ .label = "akira/diagnostics/frame_transfer", .value = milliseconds(stats.transfer_ms) });
    rows.push_back(Row{ .label = "akira/diagnostics/packets_received", .value = std::to_string(stats.packets_received) });
    rows.push_back(Row{ .label = "akira/diagnostics/packets_lost", .value = std::to_string(stats.packets_lost) });
    if (stats.console_quality_valid)
        rows.push_back(Row{ .label = "akira/diagnostics/upstream_loss",
            .value = std::format("{}%", oneDecimal(stats.upstream_loss)) });
    rows.push_back(Row{ .label = "akira/diagnostics/rejected", .value = std::to_string(stats.rejected_packets) });
    rows.push_back(Row{ .label = "akira/diagnostics/crypto_rejects", .value = std::to_string(stats.crypto_rejections) });
    rows.push_back(Row{ .label = "akira/diagnostics/reordered", .value = std::to_string(stats.reorder_skipped) });
    rows.push_back(Row{ .label = "akira/diagnostics/reorder_drops", .value = std::to_string(stats.reorder_dropped) });
    rows.push_back(Row{ .label = "akira/diagnostics/internal_drops",
        .value = std::to_string(stats.internal_drops),
        .tone = countTone(stats.internal_drops) });
    if (stats.session_errors > 0)
        rows.push_back(Row{ .label = "akira/diagnostics/session_errors",
            .value = std::to_string(stats.session_errors),
            .tone = Tone::Bad });
    return Section{ .title = "akira/diagnostics/network", .rows = std::move(rows) };
}

auto DiagnosticsReport::framesSection() const -> Section
{
    const StreamStats& stats = sample.stats;
    const std::uint64_t errors = stats.decoder_drops + stats.decoder_rejections;
    return Section{ .title = "akira/diagnostics/frames",
        .rows = { Row{ .label = "akira/diagnostics/received", .value = std::to_string(stats.frames_received) },
            Row{ .label = "akira/diagnostics/lost", .value = std::to_string(stats.network_frames_lost) },
            Row{ .label = "akira/diagnostics/patched", .value = std::to_string(stats.frames_recovered) },
            Row{ .label = "akira/diagnostics/fec_repaired", .value = std::to_string(stats.fec_repaired) },
            Row{ .label = "akira/diagnostics/keyframe_requests", .value = std::to_string(stats.keyframe_requests) },
            Row{ .label = "akira/diagnostics/keyframe_waits", .value = std::to_string(stats.keyframe_waits) },
            Row{ .label = "akira/diagnostics/unassembled", .value = std::to_string(stats.unassembled) },
            Row{ .label = "akira/diagnostics/duplicates", .value = std::to_string(stats.duplicates) },
            Row{ .label = "akira/diagnostics/stale", .value = std::to_string(stats.stale_frames) },
            Row{ .label = "akira/diagnostics/decode_errors", .value = std::to_string(errors), .tone = countTone(errors, true) } } };
}

auto DiagnosticsReport::presentationSection() const -> Section
{
    const StreamStats& stats = sample.stats;
    std::vector<Row> rows{ Row{ .label = "akira/diagnostics/source", .value = resolution(stats.video_width, stats.video_height) },
        Row{ .label = "akira/diagnostics/requested", .value = resolution(stats.requested_width, stats.requested_height) },
        Row{ .label = "akira/diagnostics/source_fps", .value = std::format("{} fps", oneDecimal(stats.source_fps)) },
        Row{ .label = "akira/diagnostics/rendered", .value = std::format("{} fps", oneDecimal(stats.fps)) },
        Row{ .label = "akira/diagnostics/decode", .value = milliseconds(stats.decode_ms) },
        Row{ .label = "akira/diagnostics/present", .value = milliseconds(stats.present_ms) },
        Row{ .label = "akira/diagnostics/decoder", .value = sample.decoder.empty() ? std::string(unavailable) : sample.decoder } };
    if (const std::optional<std::string> scaling = upscaler())
        rows.push_back(Row{ .label = "akira/diagnostics/upscaler", .value = *scaling });
    return Section{ .title = "akira/diagnostics/presentation", .rows = std::move(rows) };
}

auto DiagnosticsReport::latencySection() const -> Section
{
    const StreamStats& stats = sample.stats;
    const auto shown = [&stats](float value) { return stats.latency_valid ? milliseconds(value) : std::string(unavailable); };
    Tone tone = Tone::Neutral;
    if (stats.latency_valid)
        tone = stats.total_ms < 50.0f ? Tone::Good : stats.total_ms < 100.0f ? Tone::Warning : Tone::Bad;
    return Section{ .title = "akira/diagnostics/latency",
        .rows = { Row{ .label = "akira/diagnostics/total", .value = shown(stats.total_ms), .tone = tone },
            Row{ .label = "akira/diagnostics/net", .value = shown(stats.net_ms) },
            Row{ .label = "akira/diagnostics/visual", .value = milliseconds(stats.visual_ms) },
            Row{ .label = "akira/diagnostics/jitter", .value = milliseconds(stats.jitter_ms) } } };
}

auto DiagnosticsReport::counters() const -> std::vector<Counter>
{
    const StreamStats& stats = sample.stats;
    return { Counter{ .label = "akira/diagnostics/counter_received", .value = stats.frames_received },
        Counter{ .label = "akira/diagnostics/counter_lost", .value = stats.network_frames_lost },
        Counter{ .label = "akira/diagnostics/counter_patched", .value = stats.frames_recovered },
        Counter{ .label = "akira/diagnostics/counter_fec", .value = stats.fec_repaired },
        Counter{ .label = "akira/diagnostics/counter_keyframes", .value = stats.keyframe_requests },
        Counter{ .label = "akira/diagnostics/counter_decode_errors", .value = stats.decoder_drops + stats.decoder_rejections } };
}

auto DiagnosticsReport::budgetMilliseconds() const -> double
{
    return 1000.0 / static_cast<double>(std::max(1, sample.stats.requested_fps));
}

auto DiagnosticsReport::lossToneOf(double percent) -> Tone
{
    if (percent < 0.5)
        return Tone::Good;
    if (percent < 2.0)
        return Tone::Warning;
    return Tone::Bad;
}

auto DiagnosticsReport::countTone(std::uint64_t count, bool bad) -> Tone
{
    if (count == 0)
        return Tone::Good;
    return bad ? Tone::Bad : Tone::Warning;
}

auto DiagnosticsReport::ratioTone(double value, double target) -> Tone
{
    const double ratio = value / target;
    if (ratio >= 0.92)
        return Tone::Good;
    if (ratio >= 0.75)
        return Tone::Warning;
    return Tone::Bad;
}

auto DiagnosticsReport::oneDecimal(double value) -> std::string
{
    return std::format("{:.1f}", value);
}

auto DiagnosticsReport::millisecondsNumber(double value) -> std::string
{
    if (std::round(value * 10.0) >= 100.0)
        return std::format("{:.0f}", value);
    return oneDecimal(value);
}

auto DiagnosticsReport::milliseconds(double value) -> std::string
{
    return millisecondsNumber(value) + " ms";
}

auto DiagnosticsReport::resolution(int width, int height) -> std::string
{
    if (width <= 0 || height <= 0)
        return unavailable;
    return std::format("{}\xc3\x97{}", width, height);
}

}
