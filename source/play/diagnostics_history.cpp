#include "play/diagnostics_history.hpp"

#include <algorithm>
#include <iterator>
#include <numeric>
#include <ranges>

namespace akira::play {

void DiagnosticsHistory::record(const StreamStats& stats, double seconds)
{
    if (seconds <= 0.0)
        return;
    const StreamStats before = previous.value_or(stats);
    const auto grown = [](std::uint64_t now, std::uint64_t then) { return now > then ? now - then : 0; };
    Frame frame{
        .bitrateMbps = stats.measured_bitrate_mbps,
        .rttMilliseconds = stats.rtt_ms,
        .decodedFps = stats.source_fps,
        .renderMilliseconds = stats.visual_ms,
        .interval = Interval{ .seconds = seconds,
            .framesLost = grown(stats.network_frames_lost, before.network_frames_lost),
            .keyframeRequests = grown(stats.keyframe_requests, before.keyframe_requests),
            .keyframeWaits = grown(stats.keyframe_waits, before.keyframe_waits),
            .decodeErrors = grown(decodeErrors(stats), decodeErrors(before)) },
    };
    frame.framesLost = static_cast<double>(frame.interval.framesLost);
    if (frame.interval.framesLost > 0)
        frame.events.push_back(Event::NetworkLoss);
    if (stats.visual_ms > 0.0f && stats.visual_ms > budgetMilliseconds(stats))
        frame.events.push_back(Event::RenderOverBudget);
    if (frame.interval.decodeErrors > 0)
        frame.events.push_back(Event::DecodeError);
    frames.push_back(std::move(frame));
    while (frames.size() > capacity)
        frames.pop_front();
    previous = stats;
}

void DiagnosticsHistory::reset()
{
    frames.clear();
    previous.reset();
}

auto DiagnosticsHistory::series(double Frame::* value) const -> std::vector<double>
{
    std::vector<double> values;
    values.reserve(frames.size());
    std::ranges::transform(frames, std::back_inserter(values), [value](const Frame& frame) { return frame.*value; });
    return values;
}

auto DiagnosticsHistory::recent(std::uint64_t Interval::* value) const -> std::uint64_t
{
    const std::size_t skipped = frames.size() > window ? frames.size() - window : 0;
    return std::accumulate(frames.begin() + static_cast<std::ptrdiff_t>(skipped), frames.end(), std::uint64_t{ 0 },
        [value](std::uint64_t total, const Frame& frame) { return total + frame.interval.*value; });
}

auto DiagnosticsHistory::marks(Event event) const -> std::vector<std::size_t>
{
    const std::size_t offset = capacity - frames.size();
    std::vector<std::size_t> found;
    for (std::size_t index = 0; index < frames.size(); ++index)
        if (std::ranges::contains(frames[index].events, event))
            found.push_back(index + offset);
    return found;
}

auto DiagnosticsHistory::budgetMilliseconds(const StreamStats& stats) -> double
{
    return 1000.0 / static_cast<double>(std::max(1, stats.requested_fps));
}

auto DiagnosticsHistory::decodeErrors(const StreamStats& stats) -> std::uint64_t
{
    return stats.decoder_drops + stats.decoder_rejections;
}

}
