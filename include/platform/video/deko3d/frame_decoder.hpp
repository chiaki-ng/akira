#pragma once

#include <functional>

#include "platform/video/decoder.hpp"
#include "platform/video/deko3d/frame.hpp"

namespace akira::platform::deko3d
{

class FrameDecoder : public VideoDecoder
{
public:
    using FrameReadyCallback = std::function<void(const VideoFrame&)>;

    virtual void setFrameReadyCallback(FrameReadyCallback callback) = 0;
};

}
