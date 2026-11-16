#pragma once

#include <memory>

#include "platform/video/provider.hpp"

namespace akira::platform::viddec
{

class ViddecDecoder;

}

namespace akira::platform::software
{

class HevcDecoder;

}

namespace akira::platform::vulkan
{

class VulkanRenderer;

class VulkanVideoProvider final : public VideoProvider
{
public:
    VulkanVideoProvider();
    ~VulkanVideoProvider() override;
    VulkanVideoProvider(const VulkanVideoProvider&) = delete;
    VulkanVideoProvider& operator=(const VulkanVideoProvider&) = delete;

    const char* name() const override { return "Vulkan"; }
    std::string_view decoderName() const override;
    std::string_view decoderStat() const override;
    bool addSettings(brls::Box& group) const override;

    VideoDecoder* decoder() override;
    VideoDecoder& ensureDecoder() override;
    VideoDecoder& ensureDecoderFor(bool hevc) override;
    VideoRenderer* renderer() override;

    bool startRenderer(int width, int height, FrameHooks hooks) override;
    void detachFrames() override;
    void release() override;

private:
    std::unique_ptr<akira::platform::viddec::ViddecDecoder> m_decoder;
    std::unique_ptr<akira::platform::software::HevcDecoder> m_software;
    std::unique_ptr<VulkanRenderer> m_renderer;
};

}
