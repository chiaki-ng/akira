#include "play/picture_profile.hpp"

#include <algorithm>

#include "play/stream_settings.hpp"

namespace akira::play {

PictureProfile::PictureProfile(const StreamSettings& settings, StreamProfile profile, bool fsr4k)
    : settings(&settings)
    , chosenProfile(profile)
    , fsr4k(fsr4k)
    , chosenResolution(settings.videoResolution(profile))
    , target(settings.fsrEnabled(profile) ? offered(settings.upscaleTargetHeight(profile, chosenResolution)) : 0)
    , chosenFps(settings.videoFps(profile))
    , chosenBitrate(0)
{
    if (target > 0)
        chosenResolution = sourceFor(target);
    chosenBitrate = std::clamp(settings.videoBitrate(profile), minBitrate(), maxBitrate());
}

std::vector<VideoResolution> PictureProfile::resolutions()
{
    return {VideoResolution::P360, VideoResolution::P540, VideoResolution::P720, VideoResolution::P1080};
}

VideoResolution PictureProfile::sourceFor(int upscaleTarget)
{
    if (upscaleTarget >= 1440)
        return VideoResolution::P1080;
    if (upscaleTarget >= 1080)
        return VideoResolution::P720;
    return VideoResolution::P540;
}

std::vector<int> PictureProfile::upscaleTargets() const
{
    if (fsr4k)
        return {720, 1080, 1440, 2160};
    return {720, 1080};
}

int PictureProfile::offered(int upscaleTarget) const
{
    if (upscaleTarget <= 0)
        return 0;
    const std::vector<int> targets = upscaleTargets();
    if (std::ranges::find(targets, upscaleTarget) != targets.end())
        return upscaleTarget;
    return targets.back();
}

int PictureProfile::minBitrate() const
{
    if (chosenProfile == StreamProfile::Vpn)
        return vpnMinimumBitrate;
    return settings->minBitrate(chosenResolution);
}

int PictureProfile::maxBitrate() const
{
    if (chosenProfile == StreamProfile::Vpn)
        return vpnMaximumBitrate;
    return settings->maxBitrate(chosenResolution);
}

int PictureProfile::defaultBitrate(VideoResolution resolution) const
{
    if (chosenProfile == StreamProfile::Vpn)
        return vpnDefaultBitrate;
    return settings->defaultBitrate(resolution);
}

int PictureProfile::fitted(int bitrate) const
{
    const int snapped = ((bitrate + bitrateStep / 2) / bitrateStep) * bitrateStep;
    return std::clamp(snapped, minBitrate(), maxBitrate());
}

void PictureProfile::chooseResolution(VideoResolution resolution)
{
    target = 0;
    if (chosenResolution == resolution)
        return;
    chosenResolution = resolution;
    chosenBitrate = fitted(defaultBitrate(resolution));
}

void PictureProfile::chooseUpscale(int upscaleTarget)
{
    const int wanted = offered(upscaleTarget);
    if (wanted == target)
        return;
    target = wanted;
    if (target == 0)
        return;
    const VideoResolution source = sourceFor(target);
    if (source == chosenResolution)
        return;
    chosenResolution = source;
    chosenBitrate = fitted(defaultBitrate(source));
}

void PictureProfile::chooseFps(VideoFps fps)
{
    chosenFps = fps;
}

void PictureProfile::chooseBitrate(int bitrate)
{
    chosenBitrate = fitted(bitrate);
}

void PictureProfile::applyTo(StreamSettings& settings) const
{
    switch (chosenProfile) {
        case StreamProfile::Remote:
            settings.setRemoteVideoResolution(chosenResolution);
            settings.setRemoteFsrEnabled(upscaling());
            settings.setRemoteVideoFps(chosenFps);
            settings.setRemoteVideoBitrate(chosenBitrate);
            break;
        case StreamProfile::Vpn:
            settings.setVpnVideoResolution(chosenResolution);
            settings.setVpnFsrEnabled(upscaling());
            settings.setVpnVideoFps(chosenFps);
            settings.setVpnVideoBitrate(chosenBitrate);
            break;
        case StreamProfile::Local:
        case StreamProfile::Cloud:
            settings.setLocalVideoResolution(chosenResolution);
            settings.setLocalFsrEnabled(upscaling());
            settings.setLocalVideoFps(chosenFps);
            settings.setLocalVideoBitrate(chosenBitrate);
            break;
    }
    settings.setFsrTarget(chosenProfile, target);
}

}
