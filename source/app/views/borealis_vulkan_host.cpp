#include "app/views/borealis_vulkan_host.hpp"

#ifdef BOREALIS_USE_VULKAN

#include <borealis.hpp>

#include <mutex>

namespace akira::app::views {

auto BorealisVulkanHost::context() -> brls::SwitchVulkanVideoContext&
{
    return *static_cast<brls::SwitchVulkanVideoContext*>(brls::Application::getPlatform()->getVideoContext());
}

auto BorealisVulkanHost::physicalDevice() const -> VkPhysicalDevice
{
    return context().physicalDevice();
}

auto BorealisVulkanHost::device() const -> VkDevice
{
    return context().device();
}

auto BorealisVulkanHost::queueFamilyIndex() const -> std::uint32_t
{
    return context().queueFamilyIndex();
}

auto BorealisVulkanHost::memoryProperties() const -> const VkPhysicalDeviceMemoryProperties&
{
    return context().memoryProperties();
}

auto BorealisVulkanHost::findMemoryType(std::uint32_t typeBits, VkMemoryPropertyFlags required) const
    -> std::optional<std::uint32_t>
{
    return context().findMemoryType(typeBits, required);
}

auto BorealisVulkanHost::hasDeviceExtension(std::string_view name) const -> bool
{
    return context().hasDeviceExtension(name);
}

auto BorealisVulkanHost::colorFormat() const -> VkFormat
{
    return context().colorFormat();
}

auto BorealisVulkanHost::stencilFormat() const -> VkFormat
{
    return context().stencilFormat();
}

auto BorealisVulkanHost::displaySize() const -> DisplaySize
{
    return DisplaySize{ .width = static_cast<int>(brls::Application::windowWidth),
        .height = static_cast<int>(brls::Application::windowHeight) };
}

void BorealisVulkanHost::retireAfterFrame(std::function<void()> release)
{
    context().deferUntilFrameRetired(std::move(release));
}

auto BorealisVulkanHost::submit(const VkSubmitInfo2& submit, VkFence fence) -> VkResult
{
    std::lock_guard lock(context().queueMutex());
    return vkQueueSubmit2(context().queue(), 1, &submit, fence);
}

auto BorealisVulkanHost::waitIdle() -> VkResult
{
    std::lock_guard lock(context().queueMutex());
    return vkQueueWaitIdle(context().queue());
}

auto BorealisVulkanHost::present(platform::vulkan::FramePass& pass) -> std::unique_ptr<platform::vulkan::Presentation>
{
    return std::make_unique<Frames>(pass);
}

BorealisVulkanHost::Recorder::Recorder(brls::SwitchVulkanVideoContext& context)
    : context(context)
{
}

auto BorealisVulkanHost::Recorder::commands() -> VkCommandBuffer
{
    return context.frameCommandBuffer();
}

auto BorealisVulkanHost::Recorder::uploads() -> VkCommandBuffer
{
    return context.uploadCommandBuffer();
}

auto BorealisVulkanHost::Recorder::extent() const -> VkExtent2D
{
    return context.frameExtent();
}

auto BorealisVulkanHost::Recorder::beginRendering() -> bool
{
    if (!context.isRendering())
        context.beginRendering(VK_ATTACHMENT_LOAD_OP_LOAD);
    return context.isRendering();
}

void BorealisVulkanHost::Recorder::endRendering()
{
    context.endRendering();
}

BorealisVulkanHost::Frames::Frames(platform::vulkan::FramePass& pass)
    : pass(pass)
{
}

void BorealisVulkanHost::Frames::frameReady()
{
    VideoContext* video = brls::Application::getPlatform()->getVideoContext();
    video->beginFrame();
    Recorder recorder(context());
    pass.record(recorder);
    video->endFrame();
}

}

#endif
