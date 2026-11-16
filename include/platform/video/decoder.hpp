#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "util/av_wrappers.hpp"

namespace akira::platform
{

class VideoDecoder
{
public:
    struct Stats
    {
        float decode_ms = 0.0f;
        float source_fps = 0.0f;
        float jitter_ms = 0.0f;
        std::uint64_t decoder_drops = 0;
    };

    virtual ~VideoDecoder() = default;

    virtual bool initCodec(bool hevc, bool ps5, int width, int height) = 0;
    virtual bool initVideo(int width, int height) = 0;
    virtual bool decode(const AVBufferHandle& data, std::size_t size) = 0;
    virtual void flush() = 0;
    virtual void stalled(std::uint32_t episode)
    {
        if (episode == 1)
            flush();
    }
    virtual void cleanup() = 0;
    virtual void updateResolution(int width, int height) = 0;

    virtual int getVideoWidth() const = 0;
    virtual int getVideoHeight() const = 0;
    virtual bool isHEVC() const = 0;
    virtual bool isHardwareAccelerated() const = 0;
    virtual Stats getStats() const = 0;
    virtual std::chrono::steady_clock::time_point lastEmitTime() const = 0;
    virtual bool lastDecodeCorrupt() const = 0;
};

}
