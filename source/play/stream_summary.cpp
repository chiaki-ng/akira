#include "play/stream_summary.hpp"

#include <format>

namespace akira::play {

StreamSummary::StreamSummary(const StreamStats& stats)
    : stats(stats)
{
}

StreamSummary::Health StreamSummary::health() const
{
    if (stats.packet_loss_percent >= 2.0f)
        return Health::Poor;
    if (stats.packet_loss_percent >= 0.5f)
        return Health::Fair;
    return Health::Good;
}

std::string StreamSummary::meta() const
{
    const std::string uptime = std::format("{}m{:02}s", stats.stream_duration_seconds / 60, stats.stream_duration_seconds % 60);
    const int shownHeight = stats.video_height > 0 ? stats.video_height : stats.requested_height;
    if (shownHeight > 0 && stats.requested_fps > 0)
        return std::format("{}p{} \xc2\xb7 {}", shownHeight, stats.requested_fps, uptime);
    if (shownHeight > 0)
        return std::format("{}p \xc2\xb7 {}", shownHeight, uptime);
    if (stats.requested_fps > 0)
        return std::format("{} fps \xc2\xb7 {}", stats.requested_fps, uptime);
    return uptime;
}

}
