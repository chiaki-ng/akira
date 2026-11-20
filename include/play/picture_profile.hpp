#pragma once

#include <vector>

#include "play/stream_profile.hpp"
#include "play/video_mode.hpp"

namespace akira::play {

class StreamSettings;

class PictureProfile {
public:
    static constexpr int bitrateStep = 500;
    static constexpr int vpnMinimumBitrate = 2000;
    static constexpr int vpnMaximumBitrate = 15000;
    static constexpr int vpnDefaultBitrate = 5000;

    PictureProfile(const StreamSettings& settings, StreamProfile profile, bool fsr4k);

    static std::vector<VideoResolution> resolutions();
    static VideoResolution sourceFor(int upscaleTarget);
    std::vector<int> upscaleTargets() const;

    StreamProfile profile() const { return chosenProfile; }
    VideoResolution resolution() const { return chosenResolution; }
    int upscaleTarget() const { return target; }
    bool upscaling() const { return target > 0; }
    VideoFps fps() const { return chosenFps; }
    int bitrate() const { return chosenBitrate; }
    int minBitrate() const;
    int maxBitrate() const;

    void chooseResolution(VideoResolution resolution);
    void chooseUpscale(int upscaleTarget);
    void chooseFps(VideoFps fps);
    void chooseBitrate(int bitrate);

    void applyTo(StreamSettings& settings) const;

    friend bool operator==(const PictureProfile& left, const PictureProfile& right)
    {
        return left.chosenProfile == right.chosenProfile && left.chosenResolution == right.chosenResolution
            && left.target == right.target && left.chosenFps == right.chosenFps
            && left.chosenBitrate == right.chosenBitrate && left.fsr4k == right.fsr4k;
    }

private:
    int offered(int upscaleTarget) const;
    int defaultBitrate(VideoResolution resolution) const;
    int fitted(int bitrate) const;

    const StreamSettings* settings;
    StreamProfile chosenProfile;
    bool fsr4k;
    VideoResolution chosenResolution;
    int target;
    VideoFps chosenFps;
    int chosenBitrate;
};

}
