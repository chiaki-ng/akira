#pragma once

#include <memory>

#include "video/provider.hpp"

class VideoDecoder;
class Deko3dRenderer;

namespace akira::video
{

class Deko3dProvider final : public Provider
{
public:
    Deko3dProvider();
    ~Deko3dProvider() override;
    Deko3dProvider(const Deko3dProvider&) = delete;
    Deko3dProvider& operator=(const Deko3dProvider&) = delete;

    const char* name() const override { return "Deko3d"; }

    Decoder* decoder() override;
    Decoder& ensureDecoder() override;
    IVideoRenderer* renderer() override;

    bool startRenderer(int width, int height, FrameHooks hooks) override;
    void detachFrames() override;
    void release() override;

private:
    std::unique_ptr<VideoDecoder> m_decoder;
    std::unique_ptr<Deko3dRenderer> m_renderer;
};

}
