#include "play/stats_readout.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

namespace akira::play {

StatsReadout::StatsReadout(Sample sample)
    : sample(std::move(sample))
{
}

auto StatsReadout::codec(bool hevc) -> std::string
{
    return hevc ? "HEVC" : "H.264";
}

auto StatsReadout::graded(float value, float good, float fair) -> Tone
{
    if (value < good)
        return Tone::Good;
    return value < fair ? Tone::Warn : Tone::Bad;
}

auto StatsReadout::gradedShare(float share, float good, float fair) -> Tone
{
    if (share >= good)
        return Tone::Good;
    return share >= fair ? Tone::Warn : Tone::Bad;
}

auto StatsReadout::fps() const -> std::string
{
    return std::format("{:.1f}", sample.stats.fps);
}

auto StatsReadout::rate() const -> std::string
{
    return std::format("{:.1f}", sample.stats.measured_bitrate_mbps);
}

auto StatsReadout::loss() const -> std::string
{
    return std::format("{:.1f}", sample.stats.packet_loss_percent);
}

auto StatsReadout::rtt() const -> std::string
{
    return sample.stats.rtt_valid ? std::format("{:.0f}", sample.stats.rtt_ms) : std::string("--");
}

auto StatsReadout::totalLatency() const -> std::string
{
    return sample.stats.latency_valid ? std::format("{:.0f}", sample.stats.total_ms) : std::string("--");
}

auto StatsReadout::uptime() const -> std::string
{
    const std::uint64_t seconds = sample.stats.stream_duration_seconds;
    return std::format("{}m{:02}s", seconds / 60, seconds % 60);
}

auto StatsReadout::fpsTone() const -> Tone
{
    if (sample.stats.requested_fps <= 0)
        return Tone::Good;
    return gradedShare(sample.stats.fps / static_cast<float>(sample.stats.requested_fps), 0.92f, 0.75f);
}

auto StatsReadout::rateTone() const -> Tone
{
    if (sample.stats.requested_bitrate <= 0)
        return Tone::Good;
    return gradedShare(sample.stats.measured_bitrate_mbps / (static_cast<float>(sample.stats.requested_bitrate) / 1000.0f),
        0.7f, 0.4f);
}

auto StatsReadout::lossTone() const -> Tone
{
    return graded(sample.stats.packet_loss_percent, 0.5f, 2.0f);
}

auto StatsReadout::rttTone() const -> Tone
{
    return sample.stats.rtt_valid ? graded(sample.stats.rtt_ms, 40.0f, 80.0f) : Tone::Good;
}

auto StatsReadout::lostTone() const -> Tone
{
    const std::size_t lost = sample.stats.network_frames_lost;
    if (lost == 0)
        return Tone::Good;
    return lost <= 20 ? Tone::Warn : Tone::Bad;
}

auto StatsReadout::latencyTone() const -> Tone
{
    return sample.stats.latency_valid ? graded(sample.stats.total_ms, 50.0f, 100.0f) : Tone::Good;
}

auto StatsReadout::overall() const -> Tone
{
    return std::max({ fpsTone(), rateTone(), lossTone(), rttTone(), lostTone(), latencyTone() });
}

auto StatsReadout::compact() const -> std::vector<Cell>
{
    return {
        Cell{ .label = "FPS", .value = fps(), .tone = fpsTone() },
        Cell{ .label = "RATE", .value = rate(), .unit = "Mbps", .tone = rateTone() },
        Cell{ .label = "LOSS", .value = loss(), .unit = "%", .tone = lossTone() },
        Cell{ .label = "RTT", .value = rtt(), .unit = "ms", .tone = rttTone() },
        Cell{ .label = "LAT", .value = totalLatency(), .unit = "ms", .tone = latencyTone() },
        Cell{ .label = "UP", .value = uptime() },
    };
}

auto StatsReadout::rcasStrength() const -> int
{
    return std::clamp(static_cast<int>(std::lround((1.0f - sample.upscaling.sharpness / 2.0f) * 100.0f)), 0, 100);
}

auto StatsReadout::upscalePath() const -> std::string
{
    const UpscalingSummary& shown = sample.upscaling;
    if (!shown.active)
        return {};
    if (shown.easu)
        return std::format("{}\xe2\x80\xba{}\xe2\x80\xba{}", shown.inputHeight, shown.targetHeight, shown.displayHeight);
    return std::format("{}\xe2\x80\xba{}", shown.inputHeight, shown.displayHeight);
}

auto StatsReadout::upscaleDetail() const -> std::string
{
    const UpscalingSummary& shown = sample.upscaling;
    if (!shown.active)
        return {};
    std::string detail = shown.easu ? "EASU" : "";
    if (shown.rcas)
    {
        if (!detail.empty())
            detail += " \xc2\xb7 ";
        detail += std::format("RCAS {}%", rcasStrength());
    }
    return detail;
}

auto StatsReadout::upscaleSummary() const -> std::string
{
    const UpscalingSummary& shown = sample.upscaling;
    if (!shown.active)
        return {};
    const std::string rcas = std::format("RCAS {}%", rcasStrength());
    if (!shown.easu)
        return shown.rcas ? rcas : std::string("RCAS");
    const float scale = shown.inputHeight > 0
        ? static_cast<float>(shown.targetHeight) / static_cast<float>(shown.inputHeight)
        : 1.0f;
    std::string summary = std::format("{:.2f}\xc3\x97 {}", scale, shown.supersampling ? "SS" : "EASU");
    if (shown.rcas)
        summary += std::format(" \xc2\xb7 {}", rcas);
    return summary;
}

auto StatsReadout::context() const -> std::string
{
    if (!sample.context.empty())
        return sample.context;
    return std::format("{} \xc2\xb7 {}", sample.stats.is_hevc ? "PS5" : "PS4", sample.tunnel ? "VPN" : "Direct");
}

auto StatsReadout::requested() const -> std::vector<Cell>
{
    const StreamStats& stats = sample.stats;
    return {
        Cell{ .label = "Res", .value = std::format("{}x{}", stats.requested_width, stats.requested_height) },
        Cell{ .label = "FPS", .value = std::format("{}", stats.requested_fps) },
        Cell{ .label = "Rate", .value = std::format("{:.1f}", static_cast<float>(stats.requested_bitrate) / 1000.0f), .unit = "Mb" },
        Cell{ .label = "Codec", .value = codec(stats.requested_hevc) },
    };
}

auto StatsReadout::output() const -> std::vector<Cell>
{
    const StreamStats& stats = sample.stats;
    return {
        Cell{ .label = "Res", .value = std::format("{}x{}", stats.video_width, stats.video_height) },
        Cell{ .label = "FPS", .value = fps(), .tone = fpsTone() },
        Cell{ .label = "Path", .value = stats.is_hardware_decoder ? sample.decoder : std::string("SW") },
        Cell{ .label = "Codec", .value = codec(stats.is_hevc) },
    };
}

auto StatsReadout::link() const -> std::vector<Cell>
{
    return {
        Cell{ .label = "RTT", .value = rtt(), .unit = "ms", .tone = rttTone() },
        Cell{ .label = "Rate", .value = rate(), .unit = "Mb", .tone = rateTone() },
        Cell{ .label = "Loss", .value = loss(), .unit = "%", .tone = lossTone() },
        Cell{ .label = "Lost", .value = std::format("{}", sample.stats.network_frames_lost), .tone = lostTone() },
    };
}

auto StatsReadout::latency() const -> Cell
{
    return Cell{ .label = "LATENCY", .value = totalLatency(), .unit = "ms", .tone = latencyTone() };
}

auto StatsReadout::bar() const -> Bar
{
    const StreamStats& stats = sample.stats;
    const float total = stats.latency_valid ? stats.total_ms : stats.visual_ms;
    const float filled = std::clamp(total / latencyReferenceMs, 0.0f, 1.0f);
    const float network = stats.latency_valid && total > 0.0f ? filled * std::clamp(stats.net_ms / total, 0.0f, 1.0f) : 0.0f;
    return Bar{ .filled = filled, .network = network };
}

auto StatsReadout::legend() const -> std::vector<Cell>
{
    const StreamStats& stats = sample.stats;
    return {
        Cell{ .label = "Net", .value = stats.latency_valid ? std::format("{:.0f}", stats.net_ms) : std::string("--") },
        Cell{ .label = "Visual", .value = std::format("{:.1f}", stats.visual_ms) },
        Cell{ .label = "Decode", .value = std::format("{:.1f}", stats.decode_ms) },
        Cell{ .label = "Jitter", .value = std::format("{:.1f}", stats.jitter_ms) },
    };
}

auto StatsReadout::footer() const -> std::vector<Cell>
{
    return {
        Cell{ .label = "SRC", .value = std::format("{:.1f}", sample.stats.source_fps) },
        Cell{ .label = "GHASH", .value = "PMULL" },
        Cell{ .label = "VPN", .value = sample.tunnel.value_or("Off") },
        Cell{ .label = "REC", .value = std::format("{}", sample.stats.frames_recovered) },
    };
}

}
