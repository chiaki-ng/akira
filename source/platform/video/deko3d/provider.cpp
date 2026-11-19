#include "platform/video/deko3d/provider.hpp"
#include "log/log.hpp"

#include "platform/video/deko3d/renderer.hpp"

namespace akira::platform::deko3d
{

Deko3dVideoProvider::Deko3dVideoProvider(const char* name, Presentation presentation, DecoderChoice decoder,
    ShaderFactory makeShaders, std::optional<DecoderChoice> hevcDecoder)
    : m_name(name)
    , m_presentation(presentation)
    , m_primary_decoder(std::move(decoder))
    , m_hevc_decoder(std::move(hevcDecoder))
    , m_make_shaders(std::move(makeShaders))
{
}

Deko3dVideoProvider::~Deko3dVideoProvider() = default;

VideoDecoder* Deko3dVideoProvider::decoder()
{
    return m_decoder.get();
}

VideoDecoder& Deko3dVideoProvider::ensureDecoder()
{
    if (!m_decoder)
        m_decoder = active().make();
    return *m_decoder;
}

VideoDecoder& Deko3dVideoProvider::ensureDecoderFor(bool hevc)
{
    const bool useHevcDecoder = hevc && m_hevc_decoder.has_value();
    if (m_decoder && useHevcDecoder == m_hevc_active)
        return *m_decoder;
    m_decoder.reset();
    m_hevc_active = useHevcDecoder;
    return ensureDecoder();
}

VideoRenderer* Deko3dVideoProvider::renderer()
{
    return m_renderer.get();
}

bool Deko3dVideoProvider::startRenderer(int width, int height, FrameHooks hooks)
{
    log::info("Creating {} renderer", name());
    auto renderer = std::make_unique<Deko3dRenderer>(m_presentation.gpu, m_make_shaders(), std::string(decoderStat()),
        m_presentation.stream, m_presentation.streamState, m_presentation.wireGuard, m_presentation.palette);
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
    m_renderer.reset();
    m_decoder.reset();
    m_hevc_active = false;
}

}
