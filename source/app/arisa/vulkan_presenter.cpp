#include "app/arisa/vulkan_presenter.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <arisa/provider/gpu/frame_source.hpp>
#include <arisa/provider/surface/surface_size.hpp>

#include "app/arisa/vulkan_host.hpp"
#include "app/screenshot.hpp"
#include "log/log.hpp"

namespace akira::app::arisa {

namespace provider = ::arisa::provider;
using Clock = play::PresentLoop::Clock;

VulkanPresenter::FrameSlot::FrameSlot(VkDevice device, std::uint32_t family)
    : device(device)
{
    const VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = family,
    };
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS)
        return;
    const VkCommandBufferAllocateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    const VkFenceCreateInfo fenceInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    const VkSemaphoreCreateInfo semaphoreInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    if (vkAllocateCommandBuffers(device, &bufferInfo, &commands) != VK_SUCCESS)
        commands = VK_NULL_HANDLE;
    if (vkCreateFence(device, &fenceInfo, nullptr, &done) != VK_SUCCESS)
        done = VK_NULL_HANDLE;
    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &acquireSemaphore) != VK_SUCCESS)
        acquireSemaphore = VK_NULL_HANDLE;
}

VulkanPresenter::FrameSlot::~FrameSlot()
{
    if (pending && done != VK_NULL_HANDLE)
        vkWaitForFences(device, 1, &done, VK_TRUE, UINT64_MAX);
    if (acquireSemaphore != VK_NULL_HANDLE)
        vkDestroySemaphore(device, acquireSemaphore, nullptr);
    if (done != VK_NULL_HANDLE)
        vkDestroyFence(device, done, nullptr);
    if (pool != VK_NULL_HANDLE)
        vkDestroyCommandPool(device, pool, nullptr);
}

auto VulkanPresenter::FrameSlot::valid() const -> bool
{
    return commands != VK_NULL_HANDLE && done != VK_NULL_HANDLE && acquireSemaphore != VK_NULL_HANDLE;
}

auto VulkanPresenter::FrameSlot::idle() -> bool
{
    if (!pending)
        return true;
    if (vkGetFenceStatus(device, done) != VK_SUCCESS)
        return false;
    pending = false;
    return true;
}

auto VulkanPresenter::FrameSlot::begin(std::uint64_t frame) -> VkCommandBuffer
{
    serial = frame;
    vkResetFences(device, 1, &done);
    vkResetCommandBuffer(commands, 0);
    const VkCommandBufferBeginInfo info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(commands, &info);
    return commands;
}

VulkanPresenter::Recorder::Recorder(VkCommandBuffer commands, const Gpu::FrameTarget& frame,
    play::PresentTiming& timing)
    : buffer(commands)
    , frame(frame)
    , timing(timing)
{
}

VulkanPresenter::Recorder::~Recorder()
{
    endRendering();
}

auto VulkanPresenter::Recorder::commands() -> VkCommandBuffer
{
    return buffer;
}

auto VulkanPresenter::Recorder::uploads() -> VkCommandBuffer
{
    endRendering();
    return buffer;
}

auto VulkanPresenter::Recorder::extent() const -> VkExtent2D
{
    return frame.extent;
}

auto VulkanPresenter::Recorder::beginRendering() -> bool
{
    if (rendering)
        return true;
    const VkRenderingAttachmentInfo color{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = frame.view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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

void VulkanPresenter::Recorder::endRendering()
{
    if (!rendering)
        return;
    vkCmdEndRendering(buffer);
    rendering = false;
}

void VulkanPresenter::Recorder::copied(std::size_t bytes)
{
    timing.copied(bytes);
}

VulkanPresenter::Readback::Readback(const VulkanHost& host, VkExtent2D extent, VkFormat format,
    StreamCapture::Delivery delivery)
    : device(host.device())
    , extent(extent)
    , format(format)
    , delivery(std::move(delivery))
{
    const VkBufferCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = VkDeviceSize{ extent.width } * extent.height * Screenshot::channels,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    if (vkCreateBuffer(device, &info, nullptr, &buffer) != VK_SUCCESS)
    {
        buffer = VK_NULL_HANDLE;
        return;
    }
    VkMemoryRequirements needs{};
    vkGetBufferMemoryRequirements(device, buffer, &needs);
    const std::optional<std::uint32_t> type = host.findMemoryType(needs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!type)
        return;
    const VkMemoryAllocateInfo allocate{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = needs.size,
        .memoryTypeIndex = *type,
    };
    if (vkAllocateMemory(device, &allocate, nullptr, &memory) != VK_SUCCESS)
    {
        memory = VK_NULL_HANDLE;
        return;
    }
    if (vkBindBufferMemory(device, buffer, memory, 0) != VK_SUCCESS
        || vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS)
        mapped = nullptr;
}

VulkanPresenter::Readback::~Readback()
{
    refuse("the stream frame was not read back");
    if (mapped)
        vkUnmapMemory(device, memory);
    if (buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(device, buffer, nullptr);
    if (memory != VK_NULL_HANDLE)
        vkFreeMemory(device, memory, nullptr);
}

auto VulkanPresenter::Readback::valid() const -> bool
{
    return mapped != nullptr;
}

void VulkanPresenter::Readback::record(VkCommandBuffer commands, const Gpu::FrameTarget& frame,
    VkImageLayout presentLayout)
{
    const VkImageSubresourceRange colour{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1,
        .baseArrayLayer = 0, .layerCount = 1 };
    const VkImageMemoryBarrier2 toCopy{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = frame.image,
        .subresourceRange = colour,
    };
    const VkDependencyInfo before{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &toCopy,
    };
    vkCmdPipelineBarrier2(commands, &before);
    const VkBufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { extent.width, extent.height, 1 },
    };
    vkCmdCopyImageToBuffer(commands, frame.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
    const VkImageMemoryBarrier2 toPresent{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstAccessMask = VK_ACCESS_2_NONE,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .newLayout = presentLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = frame.image,
        .subresourceRange = colour,
    };
    const VkBufferMemoryBarrier2 toHost{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = buffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE,
    };
    const VkDependencyInfo after{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &toHost,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &toPresent,
    };
    vkCmdPipelineBarrier2(commands, &after);
}

void VulkanPresenter::Readback::armed(VkFence submitted)
{
    fence = submitted;
}

auto VulkanPresenter::Readback::ready() const -> bool
{
    return fence != VK_NULL_HANDLE && vkGetFenceStatus(device, fence) == VK_SUCCESS;
}

auto VulkanPresenter::Readback::swapsRedAndBlue(VkFormat format) -> bool
{
    return format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB;
}

void VulkanPresenter::Readback::deliver()
{
    fence = VK_NULL_HANDLE;
    const std::size_t bytes = std::size_t{ extent.width } * extent.height * Screenshot::channels;
    const auto* source = static_cast<const std::byte*>(mapped);
    std::vector<std::byte> pixels(source, source + bytes);
    if (swapsRedAndBlue(format))
        for (std::size_t pixel = 0; pixel < bytes; pixel += Screenshot::channels)
            std::swap(pixels[pixel], pixels[pixel + 2]);
    std::optional<Screenshot> shot = Screenshot::create(extent.width, extent.height, std::move(pixels));
    if (!shot)
    {
        refuse("the stream frame could not be read back");
        return;
    }
    if (delivery)
        std::exchange(delivery, nullptr)(std::move(*shot));
}

void VulkanPresenter::Readback::refuse(std::string reason)
{
    if (delivery)
        std::exchange(delivery, nullptr)(std::unexpected(std::move(reason)));
}

VulkanPresenter::VulkanPresenter(VulkanHost& host, platform::vulkan::FramePass& pass)
    : host(host)
    , pass(pass)
    , loop(*this, play::PresentLoop::Pacing{})
{
    for (std::unique_ptr<FrameSlot>& slot : slots)
        slot = std::make_unique<FrameSlot>(host.device(), host.queueFamilyIndex());
    host.attach(this);
}

VulkanPresenter::~VulkanPresenter()
{
    host.detach(this);
    host.handoff().withdraw();
    loop.stop();
    host.waitIdle();
    {
        const std::scoped_lock lock(captureMutex);
        if (wanted)
            std::exchange(wanted, std::nullopt).value()(std::unexpected(std::string("the stream ended")));
        if (reading && reading->ready())
            reading->deliver();
        reading.reset();
    }
    overlay.reset();
    log::info("[PRESENT] handing the GPU back to arisa after {} presents", takeover ? takeover->presents() : 0);
    takeover.reset();
}

void VulkanPresenter::frameReady()
{
    loop.frameReady();
}

void VulkanPresenter::paused(bool paused)
{
    loop.paused(paused);
}

void VulkanPresenter::capture(StreamCapture::Delivery delivery)
{
    const std::scoped_lock lock(captureMutex);
    if (wanted || reading)
    {
        delivery(std::unexpected(std::string("a screenshot is already being taken")));
        return;
    }
    wanted.emplace(std::move(delivery));
}

auto VulkanPresenter::awaitScene(std::chrono::milliseconds limit) -> play::PresentLoop::Scene
{
    const std::shared_ptr<const provider::FrameSource> source = scenes.load();
    if (!source || source->closed())
        return play::PresentLoop::Scene::Absent;
    const std::uint64_t published = source->await_scene(seenScene, limit);
    if (published == seenScene)
        return source->closed() ? play::PresentLoop::Scene::Absent : play::PresentLoop::Scene::Quiet;
    seenScene = published;
    return play::PresentLoop::Scene::Fresh;
}

void VulkanPresenter::follow(const Gpu::FrameTarget& frame)
{
    const provider::SurfaceSize extent{ .width = provider::SurfaceWidth{ frame.extent.width },
        .height = provider::SurfaceHeight{ frame.extent.height } };
    if (!offered && overlay)
    {
        host.handoff().offer(overlay->frames(), extent);
        offered = true;
        lentGeneration = frame.generation;
        return;
    }
    if (!offered || frame.generation == lentGeneration)
        return;
    lentGeneration = frame.generation;
    log::info("[PRESENT] the lent swapchain is now {}x{}; re-embedding the overlay", frame.extent.width,
        frame.extent.height);
    host.handoff().resize(extent);
}

void VulkanPresenter::collectReadback()
{
    const std::scoped_lock lock(captureMutex);
    if (!reading || !reading->ready())
        return;
    reading->deliver();
    reading.reset();
}

auto VulkanPresenter::startReadback(VkCommandBuffer commands, const Gpu::FrameTarget& frame, const Gpu::LentImage& lent)
    -> bool
{
    const std::scoped_lock lock(captureMutex);
    if (!wanted || reading)
        return false;
    StreamCapture::Delivery delivery = std::exchange(wanted, std::nullopt).value();
    if ((lent.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0)
    {
        delivery(std::unexpected(std::string("this surface's swapchain images cannot be copied")));
        return false;
    }
    reading.emplace(host, frame.extent, frame.format, std::move(delivery));
    if (!reading->valid())
    {
        reading->refuse("no host-visible memory for the stream frame");
        reading.reset();
        return false;
    }
    reading->record(commands, frame, lent.present_layout);
    return true;
}

void VulkanPresenter::armReadback(VkFence fence, bool submitted)
{
    const std::scoped_lock lock(captureMutex);
    if (!reading)
        return;
    if (submitted)
    {
        reading->armed(fence);
        return;
    }
    reading->refuse("the stream frame was not submitted");
    reading.reset();
}

auto VulkanPresenter::seize() -> bool
{
    if (!std::ranges::all_of(slots, [](const std::unique_ptr<FrameSlot>& slot) { return slot->valid(); }))
    {
        host.handoff().refuse("the Vulkan frame slots could not be created");
        return false;
    }
    std::expected<Gpu::Takeover, provider::ProviderError> taken = host.arisa().take_over();
    if (!taken)
    {
        const std::string reason = "arisa refused the GPU takeover: " + taken.error().explanation();
        log::error("[PRESENT] {}", reason);
        host.handoff().refuse(reason);
        return false;
    }
    takeover.emplace(std::move(*taken));
    std::expected<Gpu::Overlay, provider::ProviderError> opened = takeover->overlay();
    if (!opened)
    {
        const std::string reason = "arisa refused the stream overlay: " + opened.error().explanation();
        log::error("[PRESENT] {}", reason);
        host.handoff().refuse(reason);
        takeover.reset();
        return false;
    }
    overlay.emplace(std::move(*opened));
    scenes.store(std::make_shared<const provider::FrameSource>(overlay->frames()));
    if (interval)
        takeover->set_swap_interval(*interval);
    seized.store(true, std::memory_order_release);
    log::info("[PRESENT] took the GPU over: {} lent images, presenting from the stream thread", takeover->image_count());
    return true;
}

void VulkanPresenter::swapInterval(std::uint32_t value)
{
    interval = provider::SwapInterval::create(value);
    if (takeover && interval)
        takeover->set_swap_interval(*interval);
}

void VulkanPresenter::transition(VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to)
{
    const VkImageMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        .oldLayout = from,
        .newLayout = to,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1,
            .baseArrayLayer = 0, .layerCount = 1 },
    };
    const VkDependencyInfo dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(commands, &dependency);
}

void VulkanPresenter::clear(VkCommandBuffer commands, const Gpu::FrameTarget& frame)
{
    const VkRenderingAttachmentInfo color{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = frame.view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = { .color = { .float32 = { 0.0f, 0.0f, 0.0f, 1.0f } } },
    };
    const VkRenderingInfo info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { .offset = { 0, 0 }, .extent = frame.extent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color,
    };
    vkCmdBeginRendering(commands, &info);
    vkCmdEndRendering(commands);
}

auto VulkanPresenter::submit(VkCommandBuffer commands, const FrameSlot& slot, const Gpu::LentImage& lent,
    std::optional<Gpu::OverlayFence> fence) -> VkResult
{
    const VkSemaphoreSubmitInfo wait{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = slot.acquired(),
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    };
    std::vector<VkSemaphoreSubmitInfo> signals{ VkSemaphoreSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = lent.finished,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    } };
    if (fence)
        signals.push_back(VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = fence->semaphore,
            .value = fence->value,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        });
    const VkCommandBufferSubmitInfo buffer{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = commands,
    };
    const VkSubmitInfo2 info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = lent.waits_on_acquire ? 1u : 0u,
        .pWaitSemaphoreInfos = lent.waits_on_acquire ? &wait : nullptr,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &buffer,
        .signalSemaphoreInfoCount = static_cast<std::uint32_t>(signals.size()),
        .pSignalSemaphoreInfos = signals.data(),
    };
    return host.submit(info, slot.fence());
}

auto VulkanPresenter::present(play::PresentTiming::Kind, play::PresentTiming& timing)
    -> std::expected<play::PresentLoop::Presented, play::PresentTiming::Miss>
{
    if (!takeover)
    {
        if (refused)
            return std::unexpected(play::PresentTiming::Miss::unavailable);
        if (!seize())
        {
            refused = true;
            return std::unexpected(play::PresentTiming::Miss::unavailable);
        }
    }
    collectReadback();
    FrameSlot& slot = *slots[next];
    if (!slot.idle())
        return std::unexpected(play::PresentTiming::Miss::busy);
    host.finished(slot.lastSerial());
    const std::expected<Gpu::LentImage, provider::ProviderError> lent = takeover->acquire(slot.acquired());
    if (!lent)
    {
        log::warning("[PRESENT] no lent image: {}", lent.error().explanation());
        return std::unexpected(play::PresentTiming::Miss::unavailable);
    }
    Gpu::FrameTarget frame = lent->target;
    follow(frame);
    host.recording(++serial, frame.format);
    VkCommandBuffer commands = slot.begin(serial);
    transition(commands, frame.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    frame.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    clear(commands, frame);
    {
        Recorder recorder(commands, frame, timing);
        pass.record(recorder);
    }
    std::optional<Gpu::OverlayFence> fence;
    if (overlay)
    {
        std::expected<std::optional<Gpu::OverlayFence>, provider::ProviderError> recorded = overlay->record(commands, frame);
        if (recorded)
            fence = *recorded;
        else if (overlayFailures++ % 120 == 0)
            log::warning("[PRESENT] the overlay was not recorded: {}", recorded.error().explanation());
    }
    if (!startReadback(commands, frame, *lent))
        transition(commands, frame.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, lent->present_layout);
    vkEndCommandBuffer(commands);
    const VkResult result = submit(commands, slot, *lent, fence);
    if (result != VK_SUCCESS)
        log::error("[PRESENT] vkQueueSubmit2 failed: {}", static_cast<int>(result));
    else
        slot.submitted();
    armReadback(slot.fence(), result == VK_SUCCESS);
    const Clock::time_point submitted = Clock::now();
    if (const std::expected<void, provider::ProviderError> shown = takeover->present(frame.index); !shown)
        log::warning("[PRESENT] present refused: {}", shown.error().explanation());
    const Clock::time_point presented = Clock::now();
    next = (next + 1) % slotCount;
    return play::PresentLoop::Presented{ .submitted = submitted, .presented = presented };
}

}
