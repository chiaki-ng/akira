#pragma once

#include <chrono>
#include <cstddef>

#include "platform/video/deko3d/frame_decoder.hpp"
#include "platform/video/viddec/decoder.hpp"

namespace akira::platform::drako3d
{

class ViddecFrameDecoder final : public deko3d::FrameDecoder
{
public:
    ViddecFrameDecoder();
    ViddecFrameDecoder(const ViddecFrameDecoder&) = delete;
    ViddecFrameDecoder& operator=(const ViddecFrameDecoder&) = delete;

    void setFrameReadyCallback(FrameReadyCallback callback) override;

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
    static deko3d::VideoFrame describe(const viddec::ViddecDecoder::Frame& frame);

    viddec::ViddecDecoder m_decoder;
};

}
