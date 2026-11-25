#pragma once

#include <cstdint>
#include <string>

namespace akira::play {

enum class VideoResolution : uint32_t {
    P360 = 360,
    P540 = 540,
    P720 = 720,
    P1080 = 1080,
    P1440 = 1440,
    P2160 = 2160
};

enum class VideoFps : uint32_t {
    Fps30 = 30,
    Fps60 = 60
};

inline std::string toString(VideoResolution resolution)
{
    switch (resolution) {
        case VideoResolution::P360: return "360p";
        case VideoResolution::P540: return "540p";
        case VideoResolution::P720: return "720p";
        case VideoResolution::P1080: return "1080p";
        case VideoResolution::P1440: return "1440p";
        case VideoResolution::P2160: return "2160p";
        default: return "720p";
    }
}

inline int toInt(VideoResolution resolution)
{
    switch (resolution) {
        case VideoResolution::P360: return 360;
        case VideoResolution::P540: return 540;
        case VideoResolution::P720: return 720;
        case VideoResolution::P1080: return 1080;
        case VideoResolution::P1440: return 1440;
        case VideoResolution::P2160: return 2160;
        default: return 720;
    }
}

inline VideoResolution resolutionFromHeight(int height)
{
    if (height >= 2160) return VideoResolution::P2160;
    if (height >= 1440) return VideoResolution::P1440;
    if (height >= 1080) return VideoResolution::P1080;
    if (height >= 720) return VideoResolution::P720;
    if (height >= 540) return VideoResolution::P540;
    return VideoResolution::P360;
}

inline VideoResolution resolutionFromString(const std::string& value)
{
    if (value == "2160p") return VideoResolution::P2160;
    if (value == "1440p") return VideoResolution::P1440;
    if (value == "1080p") return VideoResolution::P1080;
    if (value == "720p") return VideoResolution::P720;
    if (value == "540p") return VideoResolution::P540;
    if (value == "360p") return VideoResolution::P360;
    return VideoResolution::P720;
}

inline std::string toString(VideoFps fps)
{
    switch (fps) {
        case VideoFps::Fps30: return "30";
        case VideoFps::Fps60: return "60";
        default: return "60";
    }
}

inline int toInt(VideoFps fps)
{
    switch (fps) {
        case VideoFps::Fps30: return 30;
        case VideoFps::Fps60: return 60;
        default: return 60;
    }
}

inline VideoFps fpsFromString(const std::string& value)
{
    if (value == "60") return VideoFps::Fps60;
    if (value == "30") return VideoFps::Fps30;
    return VideoFps::Fps60;
}

}
