#pragma once

#include <memory>

#include "platform/video/provider.hpp"

namespace akira::platform::deko3d
{

class Deko3dDecoder;
class Deko3dRenderer;

class Deko3dVideoProvider final : public VideoProvider
{
public:
    Deko3dVideoProvider();
    ~Deko3dVideoProvider() override;
    Deko3dVideoProvider(const Deko3dVideoProvider&) = delete;
    Deko3dVideoProvider& operator=(const Deko3dVideoProvider&) = delete;

    const char* name() const override { return "Deko3d"; }
    std::string_view decoderName() const override { return "FFmpeg nvtegra"; }
    std::string_view decoderStat() const override { return "NVTEGRA"; }

    VideoDecoder* decoder() override;
    VideoDecoder& ensureDecoder() override;
    VideoRenderer* renderer() override;

    bool startRenderer(int width, int height, FrameHooks hooks) override;
    void detachFrames() override;
    void release() override;

private:
    std::unique_ptr<Deko3dDecoder> m_decoder;
    std::unique_ptr<Deko3dRenderer> m_renderer;
};

}
