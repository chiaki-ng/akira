#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

#include "platform/video/decoder.hpp"

namespace akira::platform
{

class VideoRenderer;

struct VideoOptions
{
    bool softwareHevc = false;
    bool zeroCopy = false;
};

struct FrameInfo
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

class VideoProvider
{
public:
    struct FrameHooks
    {
        std::function<void(const FrameInfo&)> arriving;
        std::function<void()> presented;
    };

    virtual ~VideoProvider() = default;

    virtual const char* name() const = 0;
    virtual std::string_view decoderName() const = 0;
    virtual std::string_view decoderStat() const = 0;
    virtual VideoOptions options() const = 0;

    virtual VideoDecoder* decoder() = 0;
    virtual VideoDecoder& ensureDecoder() = 0;
    virtual VideoDecoder& ensureDecoderFor(bool) { return ensureDecoder(); }
    virtual VideoRenderer* renderer() = 0;

    virtual bool startRenderer(int width, int height, FrameHooks hooks) = 0;
    virtual void detachFrames() = 0;
    virtual void release() = 0;
};

}
