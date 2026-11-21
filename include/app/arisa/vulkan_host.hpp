#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>

#include <arisa/provider/surface/provider.hpp>
#include <arisa/provider/vulkan/vulkan_gpu.hpp>

#include "app/arisa/overlay_handoff.hpp"
#include "app/arisa/stream_capture.hpp"
#include "platform/video/vulkan/gpu_host.hpp"

namespace akira::app::arisa {

class VulkanPresenter;

class VulkanHost final : public platform::vulkan::GpuHost {
public:
    VulkanHost(::arisa::provider::vulkan::VulkanGpu& gpu, const ::arisa::provider::SurfaceProvider& surface,
        OverlayHandoff& handoff);
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

    auto arisa() -> ::arisa::provider::vulkan::VulkanGpu& { return gpu; }
    auto handoff() -> OverlayHandoff& { return overlayHandoff; }
    void recording(std::uint64_t serial, VkFormat target);
    void finished(std::uint64_t serial);
    void attach(VulkanPresenter* presenter);
    void detach(VulkanPresenter* presenter);
    [[nodiscard]] auto captureStream(StreamCapture::Delivery& delivery) -> bool;

private:
    class Retirements {
    public:
        void add(std::uint64_t serial, std::function<void()> release);
        void releaseThrough(std::uint64_t serial);
        void releaseAll();

    private:
        struct Pending {
            std::uint64_t serial;
            std::function<void()> release;
        };

        std::mutex mutex;
        std::deque<Pending> pending;
    };

    ::arisa::provider::vulkan::VulkanGpu& gpu;
    const ::arisa::provider::SurfaceProvider& surface;
    OverlayHandoff& overlayHandoff;
    VkPhysicalDeviceMemoryProperties memory{};
    std::atomic<VkFormat> format{VK_FORMAT_B8G8R8A8_UNORM};
    std::atomic<std::uint64_t> currentSerial{0};
    Retirements retirements;
    std::mutex presenting;
    VulkanPresenter* active = nullptr;
};

}
