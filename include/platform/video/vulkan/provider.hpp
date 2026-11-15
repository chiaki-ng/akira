#pragma once

#include <memory>

#include "platform/video/provider.hpp"

namespace akira::platform::vulkan
{

class VulkanDecoder;
class VulkanRenderer;

class VulkanVideoProvider final : public VideoProvider
{
public:
    VulkanVideoProvider();
    ~VulkanVideoProvider() override;
    VulkanVideoProvider(const VulkanVideoProvider&) = delete;
    VulkanVideoProvider& operator=(const VulkanVideoProvider&) = delete;

    const char* name() const override { return "Vulkan"; }
    std::string_view decoderName() const override { return "Switch 2 hardware decoder"; }
    std::string_view decoderStat() const override { return "HW"; }
    bool addSettings(brls::Box& group) const override;

    VideoDecoder* decoder() override;
    VideoDecoder& ensureDecoder() override;
    VideoRenderer* renderer() override;

    bool startRenderer(int width, int height, FrameHooks hooks) override;
    void detachFrames() override;
    void release() override;

private:
    std::unique_ptr<VulkanDecoder> m_decoder;
    std::unique_ptr<VulkanRenderer> m_renderer;
};

}
