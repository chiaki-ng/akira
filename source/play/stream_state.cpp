#include "play/stream_state.hpp"
#include "play/stream_settings.hpp"
#include "cloud/cloud_settings.hpp"
#include "settings/developer_settings.hpp"

namespace akira::play {

StreamState::StreamState(const StreamSettings& stream, const cloud::CloudSettings& cloud,
                         settings::DeveloperSettings& developer)
    : stream(stream), cloud(cloud), developer(developer)
{
}

void StreamState::setActive(bool value)
{
    active = value;
    developer.followStream(value);
}

bool StreamState::easuEnabled() const
{
    if (activeProfile == StreamProfile::Cloud)
        return cloud.fsrEnabled(pscloud);
    return stream.fsrEnabled(activeProfile);
}

int StreamState::easuTargetHeight() const
{
    const VideoResolution resolution = activeProfile == StreamProfile::Cloud
        ? resolutionFromHeight(cloud.videoResolution(pscloud))
        : stream.videoResolution(activeProfile);
    return stream.upscaleTargetHeight(activeProfile, resolution);
}

}
