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
    uint64_t mins = stats.stream_duration_seconds / 60;
    uint64_t secs = stats.stream_duration_seconds % 60;
    int shownH = stats.video_height > 0 ? stats.video_height : stats.requested_height;
    return std::format("{}p{} \xc2\xb7 {}m{:02}s", shownH, stats.requested_fps, mins, secs);
}

}
