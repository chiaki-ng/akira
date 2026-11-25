#pragma once

#ifdef BOREALIS_USE_VULKAN

#include <borealis/platforms/switch/switch_video_vk.hpp>

#include "platform/video/vulkan/gpu_host.hpp"

namespace akira::app::views {

class BorealisVulkanHost : public platform::vulkan::GpuHost {
public:
    auto physicalDevice() const -> VkPhysicalDevice override;
    auto device() const -> VkDevice override;
    auto queueFamilyIndex() const -> std::uint32_t override;
    auto memoryProperties() const -> const VkPhysicalDeviceMemoryProperties& override;
    auto findMemoryType(std::uint32_t typeBits, VkMemoryPropertyFlags required) const
        -> std::optional<std::uint32_t> override;
    auto hasDeviceExtension(std::string_view name) const -> bool override;
    auto colorFormat() const -> VkFormat override;
    auto stencilFormat() const -> VkFormat override;
    auto displaySize() const -> DisplaySize override;
    void retireAfterFrame(std::function<void()> release) override;
    auto submit(const VkSubmitInfo2& submit, VkFence fence) -> VkResult override;
    auto waitIdle() -> VkResult override;
    auto present(platform::vulkan::FramePass& pass) -> std::unique_ptr<platform::vulkan::Presentation> override;

private:
    class Recorder final : public platform::vulkan::FrameRecorder {
    public:
        explicit Recorder(brls::SwitchVulkanVideoContext& context);

        auto commands() -> VkCommandBuffer override;
        auto uploads() -> VkCommandBuffer override;
        auto extent() const -> VkExtent2D override;
        auto beginRendering() -> bool override;
        void endRendering() override;

    private:
        brls::SwitchVulkanVideoContext& context;
    };

    class Frames final : public platform::vulkan::Presentation {
    public:
        explicit Frames(platform::vulkan::FramePass& pass);

        void frameReady() override;

    private:
        platform::vulkan::FramePass& pass;
    };

    static auto context() -> brls::SwitchVulkanVideoContext&;
};

}

#endif
