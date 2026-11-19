#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>

#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/surface/provider.hpp>
#include <arisa/provider/vulkan/vulkan_gpu.hpp>

#include "platform/video/vulkan/gpu_host.hpp"

namespace akira::app::arisa {

class VulkanHost final : public platform::vulkan::GpuHost {
public:
    VulkanHost(::arisa::provider::vulkan::VulkanGpu& gpu, const ::arisa::provider::SurfaceProvider& surface);
    ~VulkanHost() override;

    VulkanHost(const VulkanHost&) = delete;
    VulkanHost& operator=(const VulkanHost&) = delete;

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
    using FrameTarget = ::arisa::provider::vulkan::VulkanGpu::FrameTarget;

    class Recorder final : public platform::vulkan::FrameRecorder {
    public:
        Recorder(VkCommandBuffer commands, const FrameTarget& frame);
        ~Recorder() override;

        Recorder(const Recorder&) = delete;
        Recorder& operator=(const Recorder&) = delete;

        auto commands() -> VkCommandBuffer override;
        auto uploads() -> VkCommandBuffer override;
        auto extent() const -> VkExtent2D override;
        auto beginRendering() -> bool override;
        void endRendering() override;

    private:
        VkCommandBuffer buffer;
        const FrameTarget& frame;
        bool rendering = false;
    };

    class Frames final : public platform::vulkan::Presentation {
    public:
        Frames(VulkanHost& host, platform::vulkan::FramePass& pass);

        void frameReady() override;

    private:
        ::arisa::provider::FrameHook hook;
    };

    class Retirements {
    public:
        void add(std::uint64_t serial, std::function<void()> release);
        void releaseCompleted(const ::arisa::provider::vulkan::VulkanGpu& gpu);
        void releaseAll();

    private:
        struct Pending {
            std::uint64_t serial;
            std::function<void()> release;
        };

        std::mutex mutex;
        std::deque<Pending> pending;
    };

    void record(platform::vulkan::FramePass& pass, VkCommandBuffer commands, const FrameTarget& frame,
        const ::arisa::provider::FrameInfo& info);

    ::arisa::provider::vulkan::VulkanGpu& gpu;
    const ::arisa::provider::SurfaceProvider& surface;
    VkPhysicalDeviceMemoryProperties memory{};
    std::atomic<VkFormat> format{VK_FORMAT_B8G8R8A8_UNORM};
    std::atomic<std::uint64_t> currentSerial{0};
    Retirements retirements;
};

}
