#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <utility>

#include "platform/video/deko3d/frame_decoder.hpp"
#include "platform/video/nv12_frame.hpp"

namespace akira::platform::drako3d
{

class Nv12FrameSource : public deko3d::FrameDecoder
{
protected:
    static deko3d::VideoFrame describe(std::shared_ptr<const Nv12Frame> frame);
};

template <class Decoder>
class Nv12FrameDecoder final : public Nv12FrameSource
{
public:
    Nv12FrameDecoder() = default;
    Nv12FrameDecoder(const Nv12FrameDecoder&) = delete;
    Nv12FrameDecoder& operator=(const Nv12FrameDecoder&) = delete;

    Decoder& decoder() { return m_decoder; }

    void setFrameReadyCallback(FrameReadyCallback callback) override
    {
        if (!callback)
        {
            m_decoder.setFrameReadyCallback(nullptr);
            return;
        }
        m_decoder.setFrameReadyCallback([callback = std::move(callback)](typename Decoder::Frame frame) {
            if (frame)
                callback(describe(std::move(frame)));
        });
    }

    bool initCodec(bool hevc, bool ps5, int width, int height) override { return m_decoder.initCodec(hevc, ps5, width, height); }
    bool initVideo(int width, int height) override { return m_decoder.initVideo(width, height); }
    bool decode(const AVBufferHandle& data, std::size_t size) override { return m_decoder.decode(data, size); }
    void flush() override { m_decoder.flush(); }
    void cleanup() override { m_decoder.cleanup(); }
    void updateResolution(int width, int height) override { m_decoder.updateResolution(width, height); }

    int getVideoWidth() const override { return m_decoder.getVideoWidth(); }
    int getVideoHeight() const override { return m_decoder.getVideoHeight(); }
    bool isHEVC() const override { return m_decoder.isHEVC(); }
    bool isHardwareAccelerated() const override { return m_decoder.isHardwareAccelerated(); }
    Stats getStats() const override { return m_decoder.getStats(); }
    std::chrono::steady_clock::time_point lastEmitTime() const override { return m_decoder.lastEmitTime(); }
    bool lastDecodeCorrupt() const override { return m_decoder.lastDecodeCorrupt(); }

private:
    Decoder m_decoder;
};

}
