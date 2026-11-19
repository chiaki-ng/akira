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

namespace akira::network
{

class WireGuardManager;

}

namespace akira::play
{

class StreamSettings;
class StreamState;

}

namespace akira::settings
{

class DeveloperSettings;

}

namespace akira::platform::vulkan
{

class VulkanRenderer;

class VulkanVideoProvider final : public VideoProvider
{
public:
    VulkanVideoProvider(play::StreamSettings& stream, const play::StreamState& streamState,
        const settings::DeveloperSettings& developer,
        const network::WireGuardManager& wireGuard);
    ~VulkanVideoProvider() override;
    VulkanVideoProvider(const VulkanVideoProvider&) = delete;
    VulkanVideoProvider& operator=(const VulkanVideoProvider&) = delete;

    const char* name() const override { return "Vulkan"; }
    std::string_view decoderName() const override;
    std::string_view decoderStat() const override;
    VideoOptions options() const override { return { .softwareHevc = true, .zeroCopy = true }; }

    VideoDecoder* decoder() override;
    VideoDecoder& ensureDecoder() override;
    VideoDecoder& ensureDecoderFor(bool hevc) override;
    VideoRenderer* renderer() override;

    bool startRenderer(int width, int height, FrameHooks hooks) override;
    void detachFrames() override;
    void release() override;

private:
    play::StreamSettings& m_stream;
    const play::StreamState& m_stream_state;
    const settings::DeveloperSettings& m_developer;
    const network::WireGuardManager& m_wire_guard;
    std::unique_ptr<akira::platform::viddec::ViddecDecoder> m_decoder;
    std::unique_ptr<akira::platform::software::HevcDecoder> m_software;
    std::unique_ptr<VulkanRenderer> m_renderer;
};

}
