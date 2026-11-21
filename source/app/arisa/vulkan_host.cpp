#include "app/arisa/vulkan_host.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/arisa/vulkan_presenter.hpp"

namespace akira::app::arisa {

namespace provider = ::arisa::provider;

VulkanHost::VulkanHost(provider::vulkan::VulkanGpu& gpu, const provider::SurfaceProvider& surface,
    OverlayHandoff& handoff)
    : gpu(gpu)
    , surface(surface)
    , overlayHandoff(handoff)
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
    return gpu.device().has_extension(name);
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
    {
        const std::scoped_lock lock(presenting);
        if (active)
        {
            retirements.add(currentSerial.load(std::memory_order_acquire), std::move(release));
            return;
        }
    }
    release();
}

void VulkanHost::recording(std::uint64_t serial, VkFormat target)
{
    currentSerial.store(serial, std::memory_order_release);
    format.store(target, std::memory_order_release);
}

void VulkanHost::finished(std::uint64_t serial)
{
    retirements.releaseThrough(serial);
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
    return std::make_unique<VulkanPresenter>(*this, pass);
}

void VulkanHost::attach(VulkanPresenter* presenter)
{
    const std::scoped_lock lock(presenting);
    active = presenter;
}

void VulkanHost::detach(VulkanPresenter* presenter)
{
    const std::scoped_lock lock(presenting);
    if (active == presenter)
        active = nullptr;
}

auto VulkanHost::captureStream(StreamCapture::Delivery& delivery) -> bool
{
    const std::scoped_lock lock(presenting);
    if (!active || !active->takenOver())
        return false;
    std::exchange(delivery, nullptr)(std::unexpected(std::string(
        "the Vulkan renderer cannot read the stream back: arisa does not say whether its lent images allow copies")));
    return true;
}

void VulkanHost::Retirements::add(std::uint64_t serial, std::function<void()> release)
{
    const std::scoped_lock lock(mutex);
    pending.push_back(Pending{ .serial = serial, .release = std::move(release) });
}

void VulkanHost::Retirements::releaseThrough(std::uint64_t serial)
{
    std::vector<std::function<void()>> done;
    {
        const std::scoped_lock lock(mutex);
        while (!pending.empty() && pending.front().serial <= serial)
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
        const std::scoped_lock lock(mutex);
        all.swap(pending);
    }
    for (auto& entry : all)
        entry.release();
}

}
