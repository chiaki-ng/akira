#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <arisa/provider/gpu/frame_source.hpp>
#include <arisa/provider/gpu/swap_interval.hpp>
#include <arisa/provider/vulkan/vulkan_gpu.hpp>

#include "app/arisa/stream_capture.hpp"
#include "platform/video/vulkan/gpu_host.hpp"
#include "play/present_loop.hpp"

namespace akira::app::arisa {

class VulkanHost;

class VulkanPresenter final : public platform::vulkan::Presentation, private play::PresentLoop::Target {
public:
    VulkanPresenter(VulkanHost& host, platform::vulkan::FramePass& pass);
    ~VulkanPresenter() override;

    VulkanPresenter(const VulkanPresenter&) = delete;
    VulkanPresenter& operator=(const VulkanPresenter&) = delete;

    void frameReady() override;
    void paused(bool paused) override;

    auto takenOver() const -> bool { return seized.load(std::memory_order_acquire); }
    void capture(StreamCapture::Delivery delivery);

private:
    using Gpu = ::arisa::provider::vulkan::VulkanGpu;

    static constexpr std::uint32_t slotCount = 3;

    class FrameSlot {
    public:
        FrameSlot(VkDevice device, std::uint32_t family);
        ~FrameSlot();

        FrameSlot(const FrameSlot&) = delete;
        FrameSlot& operator=(const FrameSlot&) = delete;

        auto valid() const -> bool;
        auto idle() -> bool;
        auto lastSerial() const -> std::uint64_t { return serial; }
        auto begin(std::uint64_t serial) -> VkCommandBuffer;
        auto acquired() const -> VkSemaphore { return acquireSemaphore; }
        auto fence() const -> VkFence { return done; }
        void submitted() { pending = true; }

    private:
        VkDevice device;
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        VkFence done = VK_NULL_HANDLE;
        VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
        std::uint64_t serial = 0;
        bool pending = false;
    };

    class Recorder final : public platform::vulkan::FrameRecorder {
    public:
        Recorder(VkCommandBuffer commands, const Gpu::FrameTarget& frame, play::PresentTiming& timing);
        ~Recorder() override;

        Recorder(const Recorder&) = delete;
        Recorder& operator=(const Recorder&) = delete;

        auto commands() -> VkCommandBuffer override;
        auto uploads() -> VkCommandBuffer override;
        auto extent() const -> VkExtent2D override;
        auto beginRendering() -> bool override;
        void endRendering() override;
        void copied(std::size_t bytes) override;

    private:
        VkCommandBuffer buffer;
        const Gpu::FrameTarget& frame;
        play::PresentTiming& timing;
        bool rendering = false;
    };

    class Readback {
    public:
        Readback(const VulkanHost& host, VkExtent2D extent, VkFormat format, StreamCapture::Delivery delivery);
        ~Readback();

        Readback(const Readback&) = delete;
        Readback& operator=(const Readback&) = delete;

        auto valid() const -> bool;
        void record(VkCommandBuffer commands, const Gpu::FrameTarget& frame, VkImageLayout presentLayout);
        void armed(VkFence fence);
        auto ready() const -> bool;
        void deliver();
        void refuse(std::string reason);

    private:
        static auto swapsRedAndBlue(VkFormat format) -> bool;

        VkDevice device;
        VkExtent2D extent;
        VkFormat format;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;
        VkFence fence = VK_NULL_HANDLE;
        StreamCapture::Delivery delivery;
    };

    auto present(play::PresentTiming::Kind kind, play::PresentTiming& timing)
        -> std::expected<play::PresentLoop::Presented, play::PresentTiming::Miss> override;
    void swapInterval(std::uint32_t interval) override;
    auto awaitScene(std::chrono::milliseconds limit) -> play::PresentLoop::Scene override;
    [[nodiscard]] auto seize() -> bool;
    void follow(const Gpu::FrameTarget& frame);
    void collectReadback();
    [[nodiscard]] auto startReadback(VkCommandBuffer commands, const Gpu::FrameTarget& frame, const Gpu::LentImage& lent)
        -> bool;
    void armReadback(VkFence fence, bool submitted);
    static void transition(VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to);
    static void clear(VkCommandBuffer commands, const Gpu::FrameTarget& frame);
    auto submit(VkCommandBuffer commands, const FrameSlot& slot, const Gpu::LentImage& lent,
        std::optional<Gpu::OverlayFence> fence) -> VkResult;

    VulkanHost& host;
    platform::vulkan::FramePass& pass;
    std::array<std::unique_ptr<FrameSlot>, slotCount> slots;
    std::uint32_t next = 0;
    std::uint64_t serial = 0;
    std::optional<Gpu::Takeover> takeover;
    std::optional<Gpu::Overlay> overlay;
    std::optional<::arisa::provider::SwapInterval> interval;
    std::atomic<std::shared_ptr<const ::arisa::provider::FrameSource>> scenes;
    std::uint64_t seenScene = 0;
    std::uint64_t lentGeneration = 0;
    bool refused = false;
    bool offered = false;
    std::uint64_t overlayFailures = 0;
    std::atomic<bool> seized{ false };
    std::mutex captureMutex;
    std::optional<StreamCapture::Delivery> wanted;
    std::optional<Readback> reading;
    play::PresentLoop loop;
};

}
