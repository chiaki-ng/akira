#include "platform/video/deko3d/provider.hpp"

#include <borealis.hpp>

#include "platform/video/deko3d/renderer.hpp"
#include "platform/video/deko3d/decoder.hpp"
#include "platform/video/deko3d/uam_shader_source.hpp"

namespace akira::platform::deko3d
{

Deko3dVideoProvider::Deko3dVideoProvider() = default;

Deko3dVideoProvider::~Deko3dVideoProvider() = default;

VideoDecoder* Deko3dVideoProvider::decoder()
{
    return m_decoder.get();
}

VideoDecoder& Deko3dVideoProvider::ensureDecoder()
{
    if (!m_decoder)
        m_decoder = std::make_unique<Deko3dDecoder>();
    return *m_decoder;
}

VideoRenderer* Deko3dVideoProvider::renderer()
{
    return m_renderer.get();
}

bool Deko3dVideoProvider::startRenderer(int width, int height, FrameHooks hooks)
{
    brls::Logger::info("Creating {} renderer", name());
    auto renderer = std::make_unique<Deko3dRenderer>(
        std::make_unique<UamShaderSource>("romfs:/shaders"), std::string(decoderStat()));
    if (!renderer->initialize(width, height))
        return false;
    m_renderer = std::move(renderer);

    m_decoder->setFrameReadyCallback([this, hooks = std::move(hooks)](const VideoFrame& frame) {
        if (!m_renderer)
            return;
        hooks.arriving(FrameInfo{ .width = static_cast<std::uint32_t>(frame.width),
            .height = static_cast<std::uint32_t>(frame.height) });
        m_renderer->presentFrame(frame);
        hooks.presented();
    });
    return true;
}

void Deko3dVideoProvider::detachFrames()
{
    if (m_decoder)
        m_decoder->setFrameReadyCallback(nullptr);
}

void Deko3dVideoProvider::release()
{
    if (m_renderer)
        m_renderer->waitIdle();
    m_decoder.reset();
    m_renderer.reset();
}

}
