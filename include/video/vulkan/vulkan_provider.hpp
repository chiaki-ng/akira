#pragma once

#include <memory>

#include "video/provider.hpp"

class StreamDecoder;
class VulkanRenderer;

namespace akira::video
{

class VulkanProvider final : public Provider
{
public:
    VulkanProvider();
    ~VulkanProvider() override;
    VulkanProvider(const VulkanProvider&) = delete;
    VulkanProvider& operator=(const VulkanProvider&) = delete;

    const char* name() const override { return "Vulkan"; }

    Decoder* decoder() override;
    Decoder& ensureDecoder() override;
    IVideoRenderer* renderer() override;

    bool startRenderer(int width, int height, FrameHooks hooks) override;
    void detachFrames() override;
    void release() override;

private:
    std::unique_ptr<StreamDecoder> m_decoder;
    std::unique_ptr<VulkanRenderer> m_renderer;
};

}
