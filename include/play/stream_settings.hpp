#pragma once

#include <array>

#include "play/renderer_kind.hpp"
#include "play/stream_profile.hpp"
#include "play/video_mode.hpp"
#include "settings/section.hpp"

namespace akira::settings {
class SettingsStore;
}

namespace akira::play {

class StreamSettings final : public settings::Section {
public:
    StreamSettings(settings::SettingsStore& store, bool fsr4k);
    ~StreamSettings() override;

    StreamSettings(const StreamSettings&) = delete;
    StreamSettings& operator=(const StreamSettings&) = delete;

    void write(toml::table& config) const override;

    VideoResolution localVideoResolution() const { return local().resolution; }
    void setLocalVideoResolution(VideoResolution value) { local().resolution = value; }
    VideoResolution remoteVideoResolution() const { return remote().resolution; }
    void setRemoteVideoResolution(VideoResolution value) { remote().resolution = value; }
    VideoResolution vpnVideoResolution() const { return vpn().resolution; }
    void setVpnVideoResolution(VideoResolution value) { vpn().resolution = value; }
    VideoResolution videoResolution(StreamProfile profile) const { return video(profile).resolution; }

    VideoFps localVideoFps() const { return local().fps; }
    void setLocalVideoFps(VideoFps value) { local().fps = value; }
    VideoFps remoteVideoFps() const { return remote().fps; }
    void setRemoteVideoFps(VideoFps value) { remote().fps = value; }
    VideoFps vpnVideoFps() const { return vpn().fps; }
    void setVpnVideoFps(VideoFps value) { vpn().fps = value; }
    VideoFps videoFps(StreamProfile profile) const { return video(profile).fps; }

    int localVideoBitrate() const { return local().bitrate; }
    void setLocalVideoBitrate(int value) { local().bitrate = value; }
    int remoteVideoBitrate() const { return remote().bitrate; }
    void setRemoteVideoBitrate(int value) { remote().bitrate = value; }
    int vpnVideoBitrate() const { return vpn().bitrate; }
    void setVpnVideoBitrate(int value) { vpn().bitrate = value; }
    int videoBitrate(StreamProfile profile) const { return video(profile).bitrate; }

    int defaultBitrate(VideoResolution resolution) const;
    int maxBitrate(VideoResolution resolution) const;
    int minBitrate(VideoResolution resolution) const;
    bool unlockBitrateMax() const { return bitrateUnlocked; }
    void setUnlockBitrateMax(bool enabled) { bitrateUnlocked = enabled; }

    bool localFsrEnabled() const { return local().fsr; }
    void setLocalFsrEnabled(bool enabled) { local().fsr = enabled; }
    bool remoteFsrEnabled() const { return remote().fsr; }
    void setRemoteFsrEnabled(bool enabled) { remote().fsr = enabled; }
    bool vpnFsrEnabled() const { return vpn().fsr; }
    void setVpnFsrEnabled(bool enabled) { vpn().fsr = enabled; }
    bool fsrEnabled(StreamProfile profile) const;
    int fsrTarget(StreamProfile profile) const;
    void setFsrTarget(StreamProfile profile, int height);
    int upscaleTargetHeight(StreamProfile profile, VideoResolution resolution) const;

    bool rcasEnabled() const { return rcas; }
    void setRcasEnabled(bool enabled) { rcas = enabled; }
    float rcasSharpness() const { return sharpness; }
    void setRcasSharpness(float value) { sharpness = value; }
    int rcasStrength() const;
    void setRcasStrength(int percent);

    bool ditheringEnabled() const { return dithering; }
    void setDitheringEnabled(bool enabled) { dithering = enabled; }
    float ditheringStrength() const { return ditherStrength; }
    void setDitheringStrength(float value);

    float statsOverlayX() const { return overlayX; }
    float statsOverlayY() const { return overlayY; }
    void setStatsOverlayPosition(float x, float y);

    bool zeroCopyVideo() const { return zeroCopy; }
    void setZeroCopyVideo(bool enabled) { zeroCopy = enabled; }
    bool softwareHevc() const { return softwareHevcDecode; }
    void setSoftwareHevc(bool enabled) { softwareHevcDecode = enabled; }
    RendererKind videoRenderer() const { return renderer; }
    void setVideoRenderer(RendererKind kind) { renderer = kind; }

    float packetLossMax() const { return packetLoss; }
    void setPacketLossMax(float value) { packetLoss = value; }
    bool requestIdrOnFecFailure() const { return idrOnFecFailure; }
    void setRequestIdrOnFecFailure(bool enabled) { idrOnFecFailure = enabled; }

private:
    struct Video {
        VideoResolution resolution;
        VideoFps fps;
        int bitrate;
        bool fsr = false;
        int fsrTarget = 0;
    };

    static int fsrTargetForResolution(VideoResolution resolution);

    void read(const toml::table& config);
    Video& local() { return videos[0]; }
    const Video& local() const { return videos[0]; }
    Video& remote() { return videos[1]; }
    const Video& remote() const { return videos[1]; }
    Video& vpn() { return videos[2]; }
    const Video& vpn() const { return videos[2]; }
    const Video& video(StreamProfile profile) const;

    settings::SettingsStore& store;
    bool fsr4k;

    std::array<Video, 3> videos{{
        {VideoResolution::P720, VideoFps::Fps60, 10000},
        {VideoResolution::P720, VideoFps::Fps60, 10000},
        {VideoResolution::P720, VideoFps::Fps30, 5000},
    }};
    bool bitrateUnlocked = false;
    bool rcas = false;
    float sharpness = 0.2f;
    bool dithering = false;
    float ditherStrength = 3.0f;
    float overlayX = -1.0f;
    float overlayY = -1.0f;
    bool zeroCopy = false;
    bool softwareHevcDecode = false;
    RendererKind renderer = RendererKind::Drako3d;
    float packetLoss = 0.05f;
    bool idrOnFecFailure = true;
};

}
