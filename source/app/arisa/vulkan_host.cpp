#include "app/arisa/vulkan_host.hpp"

#include <string_view>
#include <utility>
#include <vector>

namespace akira::app::arisa {

namespace provider = ::arisa::provider;

VulkanHost::VulkanHost(provider::vulkan::VulkanGpu& gpu, const provider::SurfaceProvider& surface)
    : gpu(gpu)
    , surface(surface)
{
    vkGetPhysicalDeviceMemoryProperties(gpu.device().physical(), &memory);
}

VulkanHost::~VulkanHost()
{
    waitIdle();
}

auto VulkanHost::physicalDevice() const -> VkPhysicalDevice
{
    return gpu.device().physical();
}

auto VulkanHost::device() const -> VkDevice
{
    return gpu.device().handle();
}

auto VulkanHost::queueFamilyIndex() const -> std::uint32_t
{
    return gpu.device().queue_family();
}

auto VulkanHost::memoryProperties() const -> const VkPhysicalDeviceMemoryProperties&
{
    return memory;
}

auto VulkanHost::findMemoryType(std::uint32_t typeBits, VkMemoryPropertyFlags required) const
    -> std::optional<std::uint32_t>
{
    for (std::uint32_t index = 0; index < memory.memoryTypeCount; ++index)
    {
        if ((typeBits & (1u << index)) != 0 && (memory.memoryTypes[index].propertyFlags & required) == required)
            return index;
    }
    return std::nullopt;
}

auto VulkanHost::hasDeviceExtension(std::string_view name) const -> bool
{
    return name == VK_KHR_SWAPCHAIN_EXTENSION_NAME;
}

auto VulkanHost::colorFormat() const -> VkFormat
{
    return format.load(std::memory_order_acquire);
}

auto VulkanHost::stencilFormat() const -> VkFormat
{
    return VK_FORMAT_UNDEFINED;
}

auto VulkanHost::displaySize() const -> DisplaySize
{
    const provider::SurfaceSize size = surface.size();
    return DisplaySize{ .width = static_cast<int>(size.width.value()), .height = static_cast<int>(size.height.value()) };
}

void VulkanHost::retireAfterFrame(std::function<void()> release)
{
    retirements.add(currentSerial.load(std::memory_order_acquire), std::move(release));
}

auto VulkanHost::submit(const VkSubmitInfo2& submit, VkFence fence) -> VkResult
{
    return gpu.device().with_queue([&submit, fence](VkQueue queue) { return vkQueueSubmit2(queue, 1, &submit, fence); });
}

auto VulkanHost::waitIdle() -> VkResult
{
    const VkResult result = gpu.device().with_queue([](VkQueue queue) { return vkQueueWaitIdle(queue); });
    retirements.releaseAll();
    return result;
}

auto VulkanHost::present(platform::vulkan::FramePass& pass) -> std::unique_ptr<platform::vulkan::Presentation>
{
    return std::make_unique<Frames>(*this, pass);
}

void VulkanHost::record(platform::vulkan::FramePass& pass, VkCommandBuffer commands, const FrameTarget& frame,
    const provider::FrameInfo& info)
{
    retirements.releaseCompleted(gpu);
    format.store(frame.format, std::memory_order_release);
    currentSerial.store(info.serial.value(), std::memory_order_release);
    Recorder recorder(commands, frame);
    pass.record(recorder);
}

VulkanHost::Recorder::Recorder(VkCommandBuffer commands, const FrameTarget& frame)
    : buffer(commands)
    , frame(frame)
{
}

VulkanHost::Recorder::~Recorder()
{
    endRendering();
}

auto VulkanHost::Recorder::commands() -> VkCommandBuffer
{
    return buffer;
}

auto VulkanHost::Recorder::uploads() -> VkCommandBuffer
{
    endRendering();
    return buffer;
}

auto VulkanHost::Recorder::extent() const -> VkExtent2D
{
    return frame.extent;
}

auto VulkanHost::Recorder::beginRendering() -> bool
{
    if (rendering)
        return true;
    const VkRenderingAttachmentInfo color{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = frame.view,
        .imageLayout = provider::vulkan::VulkanGpu::hook_layout,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };
    const VkRenderingInfo info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { .offset = { 0, 0 }, .extent = frame.extent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color,
    };
    vkCmdBeginRendering(buffer, &info);
    rendering = true;
    return true;
}

void VulkanHost::Recorder::endRendering()
{
    if (!rendering)
        return;
    vkCmdEndRendering(buffer);
    rendering = false;
}

VulkanHost::Frames::Frames(VulkanHost& host, platform::vulkan::FramePass& pass)
    : hook(host.gpu.add_frame_hook(provider::FrameStage::before_ui,
          [&host, &pass](VkCommandBuffer commands, const FrameTarget& frame, const provider::FrameInfo& info) {
              host.record(pass, commands, frame, info);
          }))
{
}

void VulkanHost::Frames::frameReady()
{
    hook.request_frame();
}

void VulkanHost::Retirements::add(std::uint64_t serial, std::function<void()> release)
{
    std::lock_guard lock(mutex);
    pending.push_back(Pending{ .serial = serial, .release = std::move(release) });
}

void VulkanHost::Retirements::releaseCompleted(const provider::vulkan::VulkanGpu& gpu)
{
    std::vector<std::function<void()>> done;
    {
        std::lock_guard lock(mutex);
        while (!pending.empty() && gpu.completed(provider::GpuSerial{pending.front().serial}))
        {
            done.push_back(std::move(pending.front().release));
            pending.pop_front();
        }
    }
    for (auto& release : done)
        release();
}

void VulkanHost::Retirements::releaseAll()
{
    std::deque<Pending> all;
    {
        std::lock_guard lock(mutex);
        all.swap(pending);
    }
    for (auto& entry : all)
        entry.release();
}

}
