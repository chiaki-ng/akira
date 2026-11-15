#include "platform/video/vulkan/provider.hpp"

#include <borealis.hpp>

#include "core/settings_manager.hpp"
#include "platform/video/vulkan/decoder.hpp"
#include "platform/video/vulkan/renderer.hpp"

namespace akira::platform::vulkan
{

VulkanVideoProvider::VulkanVideoProvider() = default;

VulkanVideoProvider::~VulkanVideoProvider() = default;

VideoDecoder* VulkanVideoProvider::decoder()
{
    return m_decoder.get();
}

VideoDecoder& VulkanVideoProvider::ensureDecoder()
{
    if (!m_decoder)
        m_decoder = std::make_unique<VulkanDecoder>();
    return *m_decoder;
}

VideoRenderer* VulkanVideoProvider::renderer()
{
    return m_renderer.get();
}

bool VulkanVideoProvider::startRenderer(int width, int height, FrameHooks hooks)
{
    brls::Logger::info("Creating {} renderer", name());
    auto renderer = std::make_unique<VulkanRenderer>();
    if (!renderer->initialize(width, height))
        return false;

    if (SettingsManager::getInstance()->getZeroCopyVideo())
        m_decoder->setOutputDevice(renderer->decoderOutputDevice());
    else
        brls::Logger::info("Zero-copy video is off, decoded frames are copied");
    m_renderer = std::move(renderer);

    m_decoder->setFrameReadyCallback([this, hooks = std::move(hooks)](VulkanDecoder::Frame frame) {
        if (!frame || !m_renderer)
            return;
        hooks.arriving(FrameInfo{ .width = frame->width(), .height = frame->height() });
        m_renderer->presentFrame(std::move(frame));
        hooks.presented();
    });
    return true;
}

void VulkanVideoProvider::detachFrames()
{
    if (m_decoder)
        m_decoder->setFrameReadyCallback(nullptr);
}

void VulkanVideoProvider::release()
{
    if (m_renderer)
        m_renderer->waitIdle();
    m_decoder.reset();
    m_renderer.reset();
}

}
