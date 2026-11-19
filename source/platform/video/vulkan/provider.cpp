#include "platform/video/vulkan/provider.hpp"
#include "log/log.hpp"

#include <borealis.hpp>

#include "play/stream_settings.hpp"
#include "settings/settings_store.hpp"
#include "platform/video/software/hevc_decoder.hpp"
#include "platform/video/viddec/decoder.hpp"
#include "platform/video/vulkan/renderer.hpp"

using namespace brls::literals;

namespace akira::platform::vulkan
{

VulkanVideoProvider::VulkanVideoProvider(play::StreamSettings& stream, const play::StreamState& streamState,
    settings::SettingsStore& store, const settings::DeveloperSettings& developer,
    const network::WireGuardManager& wireGuard)
    : m_stream(stream)
    , m_stream_state(streamState)
    , m_store(store)
    , m_developer(developer)
    , m_wire_guard(wireGuard)
{
}

VulkanVideoProvider::~VulkanVideoProvider() = default;

std::string_view VulkanVideoProvider::decoderName() const
{
    return m_software ? "Switch 2 software HEVC decoder" : "Switch 2 hardware decoder";
}

std::string_view VulkanVideoProvider::decoderStat() const
{
    return m_software ? "SW" : "HW";
}

bool VulkanVideoProvider::addSettings(brls::Box& group) const
{
    akira::platform::software::HevcDecoder::addSetting(group, m_stream, m_store);

    auto* zeroCopy = new brls::BooleanCell();
    zeroCopy->init("akira/settings/zero_copy"_i18n, m_stream.zeroCopyVideo(), [&stream = m_stream, &store = m_store](bool isOn) {
        stream.setZeroCopyVideo(isOn);
        store.save();
        log::info("Zero-copy video set to {}", isOn ? "on" : "off");
    });
    zeroCopy->setId("settings/zeroCopy");
    group.addView(zeroCopy);
    return true;
}

VideoDecoder* VulkanVideoProvider::decoder()
{
    if (m_software)
        return m_software.get();
    return m_decoder.get();
}

VideoDecoder& VulkanVideoProvider::ensureDecoder()
{
    if (m_software)
        return *m_software;
    if (!m_decoder)
        m_decoder = std::make_unique<akira::platform::viddec::ViddecDecoder>(m_developer);
    return *m_decoder;
}

VideoDecoder& VulkanVideoProvider::ensureDecoderFor(bool hevc)
{
    if (!hevc)
    {
        m_software.reset();
        return ensureDecoder();
    }
    m_decoder.reset();
    if (!m_software)
        m_software = std::make_unique<akira::platform::software::HevcDecoder>();
    return *m_software;
}

VideoRenderer* VulkanVideoProvider::renderer()
{
    return m_renderer.get();
}

bool VulkanVideoProvider::startRenderer(int width, int height, FrameHooks hooks)
{
    log::info("Creating {} renderer", name());
    auto renderer = std::make_unique<VulkanRenderer>(m_stream, m_stream_state, m_wire_guard);
    if (!renderer->initialize(width, height))
        return false;

    m_renderer = std::move(renderer);

    auto present = [this, hooks = std::move(hooks)](std::shared_ptr<const akira::platform::Nv12Frame> frame) {
        if (!frame || !m_renderer)
            return;
        hooks.arriving(FrameInfo{ .width = frame->width(), .height = frame->height() });
        m_renderer->presentFrame(std::move(frame));
        hooks.presented();
    };
    if (m_software)
    {
        m_software->setFrameReadyCallback(std::move(present));
        return true;
    }

    if (m_stream.zeroCopyVideo())
        m_decoder->setOutputMemory(m_renderer->decoderOutputMemory());
    else
        log::info("Zero-copy video is off, decoded frames are copied");
    m_decoder->setFrameReadyCallback(std::move(present));
    return true;
}

void VulkanVideoProvider::detachFrames()
{
    if (m_decoder)
        m_decoder->setFrameReadyCallback(nullptr);
    if (m_software)
        m_software->setFrameReadyCallback(nullptr);
}

void VulkanVideoProvider::release()
{
    if (m_renderer)
        m_renderer->waitIdle();
    m_decoder.reset();
    m_software.reset();
    m_renderer.reset();
}

}
