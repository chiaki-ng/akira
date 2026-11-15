#include "video/vulkan_provider.hpp"

#include <borealis.hpp>

#include "stream/stream_decoder.hpp"
#include "stream/vulkan_renderer.hpp"

namespace akira::video
{

std::unique_ptr<Provider> Provider::create()
{
    return std::make_unique<VulkanProvider>();
}

VulkanProvider::VulkanProvider() = default;

VulkanProvider::~VulkanProvider() = default;

Decoder* VulkanProvider::decoder()
{
    return m_decoder.get();
}

Decoder& VulkanProvider::ensureDecoder()
{
    if (!m_decoder)
        m_decoder = std::make_unique<StreamDecoder>();
    return *m_decoder;
}

IVideoRenderer* VulkanProvider::renderer()
{
    return m_renderer.get();
}

bool VulkanProvider::startRenderer(int width, int height, FrameHooks hooks)
{
    brls::Logger::info("Creating {} renderer", name());
    auto renderer = std::make_unique<VulkanRenderer>();
    if (!renderer->initialize(width, height))
        return false;

    m_decoder->setOutputDevice(renderer->decoderOutputDevice());
    m_renderer = std::move(renderer);

    m_decoder->setFrameReadyCallback([this, hooks = std::move(hooks)](StreamDecoder::Frame frame) {
        if (!frame || !m_renderer)
            return;
        hooks.arriving(FrameInfo{ .width = frame->width(), .height = frame->height() });
        m_renderer->presentFrame(std::move(frame));
        hooks.presented();
    });
    return true;
}

void VulkanProvider::detachFrames()
{
    if (m_decoder)
        m_decoder->setFrameReadyCallback(nullptr);
}

void VulkanProvider::release()
{
    if (m_renderer)
        m_renderer->waitIdle();
    m_decoder.reset();
    m_renderer.reset();
}

}
