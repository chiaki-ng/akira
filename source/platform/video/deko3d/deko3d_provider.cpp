#include "platform/video/deko3d/deko3d_provider.hpp"

#include <borealis.hpp>

#include "platform/video/deko3d/deko3d_renderer.hpp"
#include "platform/video/deko3d/video_decoder.hpp"

namespace akira::video
{

std::unique_ptr<Provider> Provider::create()
{
    return std::make_unique<Deko3dProvider>();
}

Provider::Capabilities Provider::capabilities()
{
    return {.hevc = true, .decoderName = "FFmpeg nvtegra", .decoderStat = "NVTEGRA"};
}

Deko3dProvider::Deko3dProvider() = default;

Deko3dProvider::~Deko3dProvider() = default;

Decoder* Deko3dProvider::decoder()
{
    return m_decoder.get();
}

Decoder& Deko3dProvider::ensureDecoder()
{
    if (!m_decoder)
        m_decoder = std::make_unique<VideoDecoder>();
    return *m_decoder;
}

IVideoRenderer* Deko3dProvider::renderer()
{
    return m_renderer.get();
}

bool Deko3dProvider::startRenderer(int width, int height, FrameHooks hooks)
{
    brls::Logger::info("Creating {} renderer", name());
    auto renderer = std::make_unique<Deko3dRenderer>();
    if (!renderer->initialize(width, height))
        return false;
    m_renderer = std::move(renderer);

    m_decoder->setFrameReadyCallback([this, hooks = std::move(hooks)](AVFrame* frame) {
        if (frame && frame->data[0] && m_renderer)
        {
            hooks.arriving(FrameInfo{ .width = static_cast<std::uint32_t>(frame->width),
                .height = static_cast<std::uint32_t>(frame->height) });
            m_renderer->presentFrame(frame);
            hooks.presented();
        }
        av_frame_free(&frame);
    });
    return true;
}

void Deko3dProvider::detachFrames()
{
    if (m_decoder)
        m_decoder->setFrameReadyCallback(nullptr);
}

void Deko3dProvider::release()
{
    if (m_renderer)
        m_renderer->waitIdle();
    m_decoder.reset();
    m_renderer.reset();
}

}
