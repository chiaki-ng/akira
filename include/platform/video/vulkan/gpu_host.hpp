#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>

#include <vulkan/vulkan.h>

namespace akira::platform::vulkan {

class FrameRecorder {
public:
    virtual ~FrameRecorder() = default;

    virtual auto commands() -> VkCommandBuffer = 0;
    virtual auto uploads() -> VkCommandBuffer = 0;
    virtual auto extent() const -> VkExtent2D = 0;
    virtual auto beginRendering() -> bool = 0;
    virtual void endRendering() = 0;
};

class FramePass {
public:
    virtual ~FramePass() = default;

    virtual void record(FrameRecorder& frame) = 0;
};

class Presentation {
public:
    virtual ~Presentation() = default;

    virtual void frameReady() = 0;
};

class GpuHost {
public:
    struct DisplaySize {
        int width;
        int height;
    };

    virtual ~GpuHost() = default;

    virtual auto physicalDevice() const -> VkPhysicalDevice = 0;
    virtual auto device() const -> VkDevice = 0;
    virtual auto queueFamilyIndex() const -> std::uint32_t = 0;
    virtual auto memoryProperties() const -> const VkPhysicalDeviceMemoryProperties& = 0;
    virtual auto findMemoryType(std::uint32_t typeBits, VkMemoryPropertyFlags required) const
        -> std::optional<std::uint32_t> = 0;
    virtual auto hasDeviceExtension(std::string_view name) const -> bool = 0;
    virtual auto colorFormat() const -> VkFormat = 0;
    virtual auto stencilFormat() const -> VkFormat = 0;
    virtual auto displaySize() const -> DisplaySize = 0;
    virtual void retireAfterFrame(std::function<void()> release) = 0;
    virtual auto submit(const VkSubmitInfo2& submit, VkFence fence) -> VkResult = 0;
    virtual auto waitIdle() -> VkResult = 0;
    virtual auto present(FramePass& pass) -> std::unique_ptr<Presentation> = 0;
};

}
