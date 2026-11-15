#include "platform/video/vulkan/provider.hpp"

#include <borealis.hpp>

#include "core/settings_manager.hpp"
#include "platform/video/vulkan/decoder.hpp"
#include "platform/video/vulkan/renderer.hpp"

using namespace brls::literals;

namespace akira::platform::vulkan
{

VulkanVideoProvider::VulkanVideoProvider() = default;

VulkanVideoProvider::~VulkanVideoProvider() = default;

bool VulkanVideoProvider::addSettings(brls::Box& group) const
{
    auto* settings = SettingsManager::getInstance();
    auto* zeroCopy = new brls::BooleanCell();
    zeroCopy->init("akira/settings/zero_copy"_i18n, settings->getZeroCopyVideo(), [settings](bool isOn) {
        settings->setZeroCopyVideo(isOn);
        settings->writeFile();
        brls::Logger::info("Zero-copy video set to {}", isOn ? "on" : "off");
    });
    group.addView(zeroCopy);

    auto* note = new brls::Label();
    note->setText("akira/settings/zero_copy_note"_i18n);
    note->setFontSize(18);
    note->setTextColor(brls::Application::getTheme().getColor("brls/text_disabled"));
    note->setMarginBottom(15);
    group.addView(note);
    return true;
}

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
