#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

#include "platform/video/decoder.hpp"

class IVideoRenderer;

namespace akira::video
{

struct FrameInfo
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

class Provider
{
public:
    struct FrameHooks
    {
        std::function<void(const FrameInfo&)> arriving;
        std::function<void()> presented;
    };

    struct Capabilities
    {
        bool hevc = false;
        std::string_view decoderName;
        std::string_view decoderStat;
    };

    static std::unique_ptr<Provider> create();
    static Capabilities capabilities();

    virtual ~Provider() = default;

    virtual const char* name() const = 0;

    virtual Decoder* decoder() = 0;
    virtual Decoder& ensureDecoder() = 0;
    virtual IVideoRenderer* renderer() = 0;

    virtual bool startRenderer(int width, int height, FrameHooks hooks) = 0;
    virtual void detachFrames() = 0;
    virtual void release() = 0;
};

}
