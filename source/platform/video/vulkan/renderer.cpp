#include "platform/video/vulkan/renderer.hpp"
#include "platform/video/vulkan/decoder_output.hpp"
#include "log/log.hpp"

#include "settings/settings_manager.hpp"
#include "network/wireguard_manager.hpp"
#include "app/views/theme.hpp"

#include <borealis/core/assets.hpp>

#include <algorithm>
#include <cstring>
#include <bit>
#include <cmath>
#include <format>
#include <limits>
#include <numeric>
#include <ranges>

extern "C"
{
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

#include "border_frag.h"
#include "easu_image_frag.h"
#include "easu_texel_frag.h"
#include "fullscreen_vert.h"
#include "overlay_frag.h"
#include "overlay_vert.h"
#include "pass_frag.h"
#include "rcas_frag.h"
#include "video_image_frag.h"
#include "video_texel_frag.h"

namespace akira::platform::vulkan
{

VulkanRenderer::PlaneImport::PlaneImport(PlaneLayout layout, std::variant<ImagePlanes, TexelPlanes> planes,
    VkDescriptorSet set)
    : m_layout(layout), m_planes(std::move(planes)), m_set(set)
{
    if (const auto* texels = std::get_if<TexelPlanes>(&m_planes))
        m_bases = texels->bases;
    else
        m_bases = {};
}

void VulkanRenderer::PlaneImport::copyFrom(const void* backing)
{
    if (auto* texels = std::get_if<TexelPlanes>(&m_planes); texels && texels->mapped && backing)
    {
        armDCacheFlush(const_cast<void*>(backing), texels->size);
        std::memcpy(texels->mapped, backing, texels->size);
    }
}

void VulkanRenderer::PlaneImport::appendAcquireBarriers(std::vector<VkImageMemoryBarrier2>& barriers)
{
    const auto* images = std::get_if<ImagePlanes>(&m_planes);
    if (!images || !m_needs_layout)
        return;

    for (VkImage image : { images->luma.get(), images->chroma.get() })
    {
        barriers.push_back(VkImageMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
            .srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        });
    }
    m_needs_layout = false;
}

VulkanRenderer::ImportCache::ImportCache(std::uint64_t generation, PlanePath path, DescriptorPool pool)
    : m_generation(generation), m_path(path), m_pool(std::move(pool))
{
}

VulkanRenderer::PlaneImport* VulkanRenderer::ImportCache::find(std::uint32_t slot)
{
    return m_slots.at(slot).get();
}

VulkanRenderer::PlaneImport& VulkanRenderer::ImportCache::store(std::uint32_t slot,
    std::unique_ptr<PlaneImport> import)
{
    m_slots.at(slot) = std::move(import);
    return *m_slots.at(slot);
}

bool VulkanRenderer::ImportCache::failed(std::uint32_t slot) const
{
    return m_failed.at(slot);
}

void VulkanRenderer::ImportCache::markFailed(std::uint32_t slot)
{
    m_failed.at(slot) = true;
}

VulkanRenderer::RenderTarget::RenderTarget(DeviceMemory memory, Image image, ImageView view, VkExtent2D extent,
    VkDescriptorSet set)
    : m_memory(std::move(memory)), m_image(std::move(image)), m_view(std::move(view)), m_extent(extent), m_set(set)
{
}

VulkanRenderer::FsrState::FsrState(FsrConfig config, DescriptorPool pool, RenderTarget easu,
    std::optional<RenderTarget> rcas)
    : m_config(config), m_pool(std::move(pool)), m_easu(std::move(easu)), m_rcas(std::move(rcas))
{
}

VulkanRenderer::OverlayTarget::OverlayTarget(DescriptorPool pool, std::array<RenderTarget, 2> colors,
    RenderTarget stencil)
    : m_pool(std::move(pool)), m_colors(std::move(colors)), m_stencil(std::move(stencil))
{
}

VulkanRenderer::OverlayContext::OverlayContext(CommandPool pool, std::array<VkCommandBuffer, 2> commands,
    Fence fence, std::unique_ptr<nvg::VkRenderer> renderer, NVGcontext* vg, int font)
    : m_pool(std::move(pool)), m_commands(commands), m_fence(std::move(fence)), m_renderer(std::move(renderer)),
      m_vg(vg), m_font(font)
{
}

VulkanRenderer::OverlayContext::~OverlayContext()
{
    if (m_vg)
        nvg::VkRenderer::deleteContext(m_vg);
}

VulkanRenderer::VulkanRenderer() = default;

VulkanRenderer::~VulkanRenderer()
{
    cleanup();
}

bool VulkanRenderer::succeeded(VkResult result, std::string_view what)
{
    if (result == VK_SUCCESS)
        return true;
    akira::log::error("VulkanRenderer: {} failed ({})", what, static_cast<int>(result));
    return false;
}

float VulkanRenderer::ovlSize(float px)
{
    if (px > 1.0f && px < 200.0f)
        return px;
    return px <= 1.0f ? 8.0f : 24.0f;
}

NVGcolor VulkanRenderer::ovl(NVGcolor color, unsigned char alpha)
{
    return app::views::withAlpha(color, alpha);
}

VkImageAspectFlags VulkanRenderer::stencilAspect(VkFormat format)
{
    return format == VK_FORMAT_S8_UINT ? VK_IMAGE_ASPECT_STENCIL_BIT
                                       : VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
}

void VulkanRenderer::imageBarrier(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect,
    VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
    VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess)
{
    const VkImageMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = srcStage,
        .srcAccessMask = srcAccess,
        .dstStageMask = dstStage,
        .dstAccessMask = dstAccess,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = { aspect, 0, 1, 0, 1 },
    };
    const VkDependencyInfo dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(cmd, &dependency);
}

void VulkanRenderer::setViewport(VkCommandBuffer cmd, VkExtent2D extent)
{
    const VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    const VkRect2D scissor{ { 0, 0 }, extent };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

VulkanRenderer::PlaneLayout VulkanRenderer::planeLayout(const akira::platform::Nv12Frame& frame) const
{
    return PlaneLayout{
        .backing = frame.backing(),
        .backingSize = frame.backingSize(),
        .width = frame.width(),
        .height = frame.height(),
        .luma = frame.luma(),
        .chroma = frame.chroma(),
        .decoderBuffer = m_decoder_sampling == DecoderSampling::Broken
            ? VK_NULL_HANDLE
            : frame.output().transform(&akira::platform::vulkan::DecoderOutput::buffer).value_or(VK_NULL_HANDLE),
    };
}

bool VulkanRenderer::decoderSamplingMatches(const PlaneLayout& layout)
{
    const std::uint32_t rows = std::min(layout.height, SamplingProbeRows);
    const VkDeviceSize offset = layout.luma.offset + VkDeviceSize{ layout.luma.pitch } * ((layout.height - rows) / 2);
    const VkDeviceSize size = VkDeviceSize{ layout.luma.pitch } * (rows - 1) + layout.width;
    if (rows == 0 || offset + size > layout.backingSize)
        return false;

    const VkBufferCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer bufferHandle = VK_NULL_HANDLE;
    if (!succeeded(vkCreateBuffer(m_device, &info, nullptr, &bufferHandle), "vkCreateBuffer (sampling probe)"))
        return false;
    Buffer buffer(m_device, bufferHandle);
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(m_device, buffer.get(), &requirements);
    const auto type = m_vctx->findMemoryType(requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!type)
        return false;
    const VkMemoryAllocateInfo allocation{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = *type,
    };
    VkDeviceMemory memoryHandle = VK_NULL_HANDLE;
    if (!succeeded(vkAllocateMemory(m_device, &allocation, nullptr, &memoryHandle), "vkAllocateMemory (sampling probe)"))
        return false;
    DeviceMemory memory(m_device, memoryHandle);
    void* mapped = nullptr;
    if (!succeeded(vkBindBufferMemory(m_device, buffer.get(), memory.get(), 0), "vkBindBufferMemory (sampling probe)") ||
        !succeeded(vkMapMemory(m_device, memory.get(), 0, VK_WHOLE_SIZE, 0, &mapped), "vkMapMemory (sampling probe)"))
        return false;

    const VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = m_vctx->queueFamilyIndex(),
    };
    VkCommandPool poolHandle = VK_NULL_HANDLE;
    if (!succeeded(vkCreateCommandPool(m_device, &poolInfo, nullptr, &poolHandle), "vkCreateCommandPool (sampling probe)"))
        return false;
    CommandPool pool(m_device, poolHandle);
    const VkCommandBufferAllocateInfo commandInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool.get(),
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (!succeeded(vkAllocateCommandBuffers(m_device, &commandInfo, &cmd), "vkAllocateCommandBuffers (sampling probe)"))
        return false;
    const VkFenceCreateInfo fenceInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fenceHandle = VK_NULL_HANDLE;
    if (!succeeded(vkCreateFence(m_device, &fenceInfo, nullptr, &fenceHandle), "vkCreateFence (sampling probe)"))
        return false;
    Fence fence(m_device, fenceHandle);

    const VkCommandBufferBeginInfo begin{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    if (!succeeded(vkBeginCommandBuffer(cmd, &begin), "vkBeginCommandBuffer (sampling probe)"))
        return false;
    const VkBufferMemoryBarrier2 decoderWrite{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
        .dstQueueFamilyIndex = m_vctx->queueFamilyIndex(),
        .buffer = layout.decoderBuffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE,
    };
    const VkDependencyInfo acquire{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &decoderWrite,
    };
    vkCmdPipelineBarrier2(cmd, &acquire);
    const VkBufferCopy region{ .srcOffset = offset, .dstOffset = 0, .size = size };
    vkCmdCopyBuffer(cmd, layout.decoderBuffer, buffer.get(), 1, &region);
    const VkMemoryBarrier2 hostRead{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
    };
    const VkDependencyInfo release{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &hostRead,
    };
    vkCmdPipelineBarrier2(cmd, &release);
    if (!succeeded(vkEndCommandBuffer(cmd), "vkEndCommandBuffer (sampling probe)"))
        return false;

    const VkCommandBufferSubmitInfo commandSubmit{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmd,
    };
    const VkSubmitInfo2 submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &commandSubmit,
    };
    {
        std::lock_guard lock(m_vctx->queueMutex());
        if (!succeeded(vkQueueSubmit2(m_vctx->queue(), 1, &submit, fence.get()), "vkQueueSubmit2 (sampling probe)"))
            return false;
    }
    if (!succeeded(vkWaitForFences(m_device, 1, &fenceHandle, VK_TRUE, SamplingProbeTimeoutNs), "vkWaitForFences (sampling probe)"))
    {
        std::lock_guard lock(m_vctx->queueMutex());
        vkQueueWaitIdle(m_vctx->queue());
        return false;
    }

    auto* decoded = static_cast<std::uint8_t*>(const_cast<void*>(layout.backing)) + offset;
    armDCacheFlush(decoded, static_cast<std::size_t>(size));
    return std::memcmp(mapped, decoded, static_cast<std::size_t>(size)) == 0;
}

std::shared_ptr<akira::platform::viddec::OutputMemory> VulkanRenderer::decoderOutputMemory() const
{
    if (!m_vctx || m_path != PlanePath::TexelBuffer)
        return nullptr;
    return std::make_shared<akira::platform::vulkan::DecoderOutput>(m_device, m_vctx->physicalDevice());
}

VkShaderStageFlags VulkanRenderer::pushStages(Program program)
{
    return program == Program::Video || program == Program::Easu
        ? VkShaderStageFlags{VK_SHADER_STAGE_FRAGMENT_BIT}
        : VkShaderStageFlags{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT};
}

VulkanRenderer::FsrConfig VulkanRenderer::fsrConfig(int inputWidth, int inputHeight, VkExtent2D display)
{
    auto* settings = SettingsManager::getInstance();
    FsrConfig config{
        .rcas = settings->getRcasEnabled(),
        .dithering = settings->getEnableDithering(),
        .sharpness = settings->getRcasSharpness(),
        .inputWidth = inputWidth,
        .inputHeight = inputHeight,
        .display = display,
    };

    const int displayWidth = static_cast<int>(display.width);
    const int displayHeight = static_cast<int>(display.height);
    const int targetH = settings->getEasuTargetHeight();
    const int targetW = (targetH * 16) / 9;
    if (settings->getEasuEnabled() && targetH > 0 && (inputWidth < targetW || inputHeight < targetH))
    {
        config.easu = true;
        config.targetWidth = targetW;
        config.targetHeight = targetH;
    }
    else
    {
        config.targetWidth = displayWidth;
        config.targetHeight = displayHeight;
    }

    config.supersampling = config.targetWidth > displayWidth || config.targetHeight > displayHeight;
    config.rcasTarget = config.rcas && (config.targetWidth != displayWidth || config.targetHeight != displayHeight);

    const float inputW = static_cast<float>(inputWidth);
    const float inputH = static_cast<float>(inputHeight);
    const float outputW = static_cast<float>(config.targetWidth);
    const float outputH = static_cast<float>(config.targetHeight);
    const auto bits = [](float value) { return std::bit_cast<std::uint32_t>(value); };

    config.easuConstants = {
        bits(inputW / outputW), bits(inputH / outputH),
        bits(0.5f * inputW / outputW - 0.5f), bits(0.5f * inputH / outputH - 0.5f),
        bits(1.0f / inputW), bits(1.0f / inputH), bits(1.0f / inputW), bits(-1.0f / inputH),
        bits(-1.0f / inputW), bits(2.0f / inputH), bits(1.0f / inputW), bits(2.0f / inputH),
        bits(0.0f / inputW), bits(4.0f / inputH), 0, 0,
    };
    config.rcasConstants = { bits(std::exp2(-config.sharpness)), 0, 0, 0 };
    return config;
}

bool VulkanRenderer::initialize(int frame_width, int frame_height)
{
    if (m_initialized.load(std::memory_order_acquire))
        return true;

    akira::log::info("VulkanRenderer::initialize: frame={}x{}", frame_width, frame_height);

    m_vctx = static_cast<brls::SwitchVulkanVideoContext*>(brls::Application::getPlatform()->getVideoContext());
    if (!m_vctx)
    {
        akira::log::error("VulkanRenderer: no SwitchVulkanVideoContext");
        return false;
    }
    m_device = m_vctx->device();

    if (!queryCapabilities() || !createSharedObjects())
        return false;

    const float saved_x = SettingsManager::getInstance()->getStatsOverlayX();
    const float saved_y = SettingsManager::getInstance()->getStatsOverlayY();
    if (saved_x >= 0.0f && saved_y >= 0.0f)
    {
        m_overlay_norm_x.store(saved_x, std::memory_order_relaxed);
        m_overlay_norm_y.store(saved_y, std::memory_order_relaxed);
    }

    m_dithering_enabled.store(SettingsManager::getInstance()->getEnableDithering(), std::memory_order_relaxed);
    m_initialized.store(true, std::memory_order_release);
    akira::log::info("VulkanRenderer: initialized (dithering={})", m_dithering_enabled.load());
    return true;
}

bool VulkanRenderer::queryCapabilities()
{
    if (!m_vctx->hasDeviceExtension(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME))
    {
        akira::log::error("VulkanRenderer: {} is not enabled, decoded frames cannot be sampled in place",
            VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
        return false;
    }

    m_get_host_pointer_properties = reinterpret_cast<PFN_vkGetMemoryHostPointerPropertiesEXT>(
        vkGetDeviceProcAddr(m_device, "vkGetMemoryHostPointerPropertiesEXT"));
    if (!m_get_host_pointer_properties)
    {
        akira::log::error("VulkanRenderer: vkGetMemoryHostPointerPropertiesEXT is unavailable");
        return false;
    }

    VkPhysicalDeviceExternalMemoryHostPropertiesEXT host{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT,
    };
    VkPhysicalDeviceProperties2 properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &host,
    };
    vkGetPhysicalDeviceProperties2(m_vctx->physicalDevice(), &properties);

    m_caps.hostPointerAlignment = std::max<VkDeviceSize>(host.minImportedHostPointerAlignment, 1);
    m_caps.maxTexelBufferElements = properties.properties.limits.maxTexelBufferElements;
    m_caps.texelOffsetAlignment = std::max<VkDeviceSize>(properties.properties.limits.minTexelBufferOffsetAlignment, 1);
    const auto imageExtent = m_vctx->hasDeviceExtension(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME)
        ? queryImagePlanes()
        : std::nullopt;
    m_caps.imagePlanes = imageExtent.has_value();
    m_caps.imageMaxExtent = imageExtent.value_or(VkExtent3D{});
    m_caps.texelPlanes = queryTexelPlanes();

    if (m_caps.texelPlanes)
        m_path = PlanePath::TexelBuffer;
    else
    {
        akira::log::error("VulkanRenderer: neither linear host images nor host texel buffers can be imported, "
            "decoded frames cannot be sampled in place");
        return false;
    }

    akira::log::info("VulkanRenderer: decoded planes are sampled through uniform texel buffers");
    return true;
}

std::optional<VkExtent3D> VulkanRenderer::queryImagePlanes() const
{
    VkExtent3D maxExtent{
        std::numeric_limits<std::uint32_t>::max(),
        std::numeric_limits<std::uint32_t>::max(),
        1,
    };

    for (VkFormat format : { VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM })
    {
        VkDrmFormatModifierPropertiesListEXT list{
            .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
        };
        VkFormatProperties2 formatProperties{
            .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
            .pNext = &list,
        };
        vkGetPhysicalDeviceFormatProperties2(m_vctx->physicalDevice(), format, &formatProperties);
        std::vector<VkDrmFormatModifierPropertiesEXT> modifiers(list.drmFormatModifierCount);
        list.pDrmFormatModifierProperties = modifiers.data();
        vkGetPhysicalDeviceFormatProperties2(m_vctx->physicalDevice(), format, &formatProperties);

        constexpr VkFormatFeatureFlags required =
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        const auto linear = std::ranges::find(modifiers, LinearModifier,
            &VkDrmFormatModifierPropertiesEXT::drmFormatModifier);
        if (linear == modifiers.end() || linear->drmFormatModifierPlaneCount != 1 ||
            (linear->drmFormatModifierTilingFeatures & required) != required)
        {
            akira::log::info("VulkanRenderer: format {} has no sampleable linear DRM modifier",
                static_cast<int>(format));
            return std::nullopt;
        }

        const VkPhysicalDeviceImageDrmFormatModifierInfoEXT modifierInfo{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
            .drmFormatModifier = LinearModifier,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        const VkPhysicalDeviceExternalImageFormatInfo externalInfo{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
            .pNext = &modifierInfo,
            .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
        };
        const VkPhysicalDeviceImageFormatInfo2 info{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
            .pNext = &externalInfo,
            .format = format,
            .type = VK_IMAGE_TYPE_2D,
            .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
        };
        VkExternalImageFormatProperties external{
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
        };
        VkImageFormatProperties2 imageProperties{
            .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
            .pNext = &external,
        };
        const VkResult result =
            vkGetPhysicalDeviceImageFormatProperties2(m_vctx->physicalDevice(), &info, &imageProperties);
        if (result != VK_SUCCESS)
        {
            akira::log::info("VulkanRenderer: linear host-memory images of format {} are unsupported ({})",
                static_cast<int>(format), static_cast<int>(result));
            return std::nullopt;
        }
        if ((external.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) == 0)
        {
            akira::log::info("VulkanRenderer: format {} images cannot import host allocations",
                static_cast<int>(format));
            return std::nullopt;
        }

        maxExtent.width = std::min(maxExtent.width, imageProperties.imageFormatProperties.maxExtent.width);
        maxExtent.height = std::min(maxExtent.height, imageProperties.imageFormatProperties.maxExtent.height);
    }

    return maxExtent;
}

bool VulkanRenderer::queryTexelPlanes() const
{
    for (VkFormat format : { VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM })
    {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(m_vctx->physicalDevice(), format, &properties);
        if ((properties.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT) == 0)
        {
            akira::log::info("VulkanRenderer: format {} cannot back a uniform texel buffer", static_cast<int>(format));
            return false;
        }
    }

    return true;
}

bool VulkanRenderer::createSharedObjects()
{
    const std::array<std::span<const std::uint32_t>, ShaderCount> codes{
        std::span<const std::uint32_t>(fullscreen_vert),
        std::span<const std::uint32_t>(overlay_vert),
        std::span<const std::uint32_t>(video_image_frag),
        std::span<const std::uint32_t>(video_texel_frag),
        std::span<const std::uint32_t>(easu_image_frag),
        std::span<const std::uint32_t>(easu_texel_frag),
        std::span<const std::uint32_t>(rcas_frag),
        std::span<const std::uint32_t>(pass_frag),
        std::span<const std::uint32_t>(overlay_frag),
        std::span<const std::uint32_t>(border_frag),
    };
    for (auto [index, code] : std::views::enumerate(codes))
    {
        auto module = createShaderModule(code);
        if (!module)
            return false;
        m_shared.shaders[static_cast<std::size_t>(index)] = std::move(*module);
    }

    const VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
    };
    VkSampler sampler = VK_NULL_HANDLE;
    if (!succeeded(vkCreateSampler(m_device, &samplerInfo, nullptr, &sampler), "vkCreateSampler"))
        return false;
    m_shared.sampler = Sampler(m_device, sampler);

    auto imageSetLayout = createSetLayout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2);
    auto texelSetLayout = createSetLayout(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 2);
    auto textureSetLayout = createSetLayout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
    if (!imageSetLayout || !texelSetLayout || !textureSetLayout)
        return false;
    m_shared.imageSetLayout = std::move(*imageSetLayout);
    m_shared.texelSetLayout = std::move(*texelSetLayout);
    m_shared.textureSetLayout = std::move(*textureSetLayout);

    auto imagePipelineLayout = createPipelineLayout(m_shared.imageSetLayout.get(), pushStages(Program::Video),
        sizeof(VideoPush));
    auto texelPipelineLayout = createPipelineLayout(m_shared.texelSetLayout.get(), pushStages(Program::Video),
        sizeof(VideoPush));
    auto texturePipelineLayout = createPipelineLayout(m_shared.textureSetLayout.get(), pushStages(Program::Pass),
        sizeof(QuadPush));
    if (!imagePipelineLayout || !texelPipelineLayout || !texturePipelineLayout)
        return false;
    m_shared.imagePipelineLayout = std::move(*imagePipelineLayout);
    m_shared.texelPipelineLayout = std::move(*texelPipelineLayout);
    m_shared.texturePipelineLayout = std::move(*texturePipelineLayout);
    return true;
}

std::optional<VulkanRenderer::ShaderModule> VulkanRenderer::createShaderModule(std::span<const std::uint32_t> code) const
{
    const VkShaderModuleCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size_bytes(),
        .pCode = code.data(),
    };
    VkShaderModule module = VK_NULL_HANDLE;
    if (!succeeded(vkCreateShaderModule(m_device, &info, nullptr, &module), "vkCreateShaderModule"))
        return std::nullopt;
    return ShaderModule(m_device, module);
}

std::optional<VulkanRenderer::DescriptorSetLayout> VulkanRenderer::createSetLayout(VkDescriptorType type,
    std::uint32_t bindings) const
{
    const std::array entries{
        VkDescriptorSetLayoutBinding{ 0, type, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        VkDescriptorSetLayoutBinding{ 1, type, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
    };
    const VkDescriptorSetLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = std::min<std::uint32_t>(bindings, entries.size()),
        .pBindings = entries.data(),
    };
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    if (!succeeded(vkCreateDescriptorSetLayout(m_device, &info, nullptr, &layout), "vkCreateDescriptorSetLayout"))
        return std::nullopt;
    return DescriptorSetLayout(m_device, layout);
}

std::optional<VulkanRenderer::PipelineLayout> VulkanRenderer::createPipelineLayout(VkDescriptorSetLayout setLayout,
    VkShaderStageFlags stages, std::uint32_t pushSize) const
{
    const VkPushConstantRange range{ stages, 0, pushSize };
    const VkPipelineLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &setLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &range,
    };
    VkPipelineLayout layout = VK_NULL_HANDLE;
    if (!succeeded(vkCreatePipelineLayout(m_device, &info, nullptr, &layout), "vkCreatePipelineLayout"))
        return std::nullopt;
    return PipelineLayout(m_device, layout);
}

std::optional<VulkanRenderer::DescriptorPool> VulkanRenderer::createDescriptorPool(VkDescriptorType type,
    std::uint32_t sets, std::uint32_t descriptors) const
{
    const VkDescriptorPoolSize size{ type, descriptors };
    const VkDescriptorPoolCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = sets,
        .poolSizeCount = 1,
        .pPoolSizes = &size,
    };
    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (!succeeded(vkCreateDescriptorPool(m_device, &info, nullptr, &pool), "vkCreateDescriptorPool"))
        return std::nullopt;
    return DescriptorPool(m_device, pool);
}

std::optional<VkDescriptorSet> VulkanRenderer::allocateSet(VkDescriptorPool pool, VkDescriptorSetLayout layout) const
{
    const VkDescriptorSetAllocateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };
    VkDescriptorSet set = VK_NULL_HANDLE;
    if (!succeeded(vkAllocateDescriptorSets(m_device, &info, &set), "vkAllocateDescriptorSets"))
        return std::nullopt;
    return set;
}

std::optional<VulkanRenderer::RenderTarget> VulkanRenderer::createRenderTarget(VkExtent2D extent, VkFormat format,
    VkImageUsageFlags usage, VkImageAspectFlags aspect, VkDescriptorPool pool) const
{
    const VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = { extent.width, extent.height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage imageHandle = VK_NULL_HANDLE;
    if (!succeeded(vkCreateImage(m_device, &imageInfo, nullptr, &imageHandle), "vkCreateImage (render target)"))
        return std::nullopt;
    Image image(m_device, imageHandle);

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(m_device, image.get(), &requirements);
    auto type = m_vctx->findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!type)
        type = m_vctx->findMemoryType(requirements.memoryTypeBits, 0);
    if (!type)
    {
        akira::log::error("VulkanRenderer: no memory type for a {}x{} render target", extent.width, extent.height);
        return std::nullopt;
    }

    const VkMemoryAllocateInfo allocation{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = *type,
    };
    VkDeviceMemory memoryHandle = VK_NULL_HANDLE;
    if (!succeeded(vkAllocateMemory(m_device, &allocation, nullptr, &memoryHandle), "vkAllocateMemory (render target)"))
        return std::nullopt;
    DeviceMemory memory(m_device, memoryHandle);
    if (!succeeded(vkBindImageMemory(m_device, image.get(), memory.get(), 0), "vkBindImageMemory (render target)"))
        return std::nullopt;

    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image.get(),
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = { aspect, 0, 1, 0, 1 },
    };
    VkImageView viewHandle = VK_NULL_HANDLE;
    if (!succeeded(vkCreateImageView(m_device, &viewInfo, nullptr, &viewHandle), "vkCreateImageView (render target)"))
        return std::nullopt;
    ImageView view(m_device, viewHandle);

    VkDescriptorSet set = VK_NULL_HANDLE;
    if (pool != VK_NULL_HANDLE)
    {
        const auto allocated = allocateSet(pool, m_shared.textureSetLayout.get());
        if (!allocated)
            return std::nullopt;
        set = *allocated;
        const VkDescriptorImageInfo descriptor{ m_shared.sampler.get(), view.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        const VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &descriptor,
        };
        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    }

    return RenderTarget(std::move(memory), std::move(image), std::move(view), extent, set);
}

std::optional<VulkanRenderer::HostRange> VulkanRenderer::hostRange(const PlaneLayout& layout) const
{
    if (!layout.backing || layout.width == 0 || layout.height == 0)
    {
        akira::log::error("VulkanRenderer: decoder frame has no backing memory or an empty size");
        return std::nullopt;
    }
    const VkDeviceSize required = std::max(layout.lumaEnd(), layout.chromaEnd());
    if (required > layout.backingSize)
    {
        akira::log::error("VulkanRenderer: decoder backing of {:#x} bytes is short of the {:#x} bytes the NV12 planes "
            "span", layout.backingSize, required);
        return std::nullopt;
    }
    return HostRange{ ~0U, required };
}

std::optional<VulkanRenderer::DeviceMemory> VulkanRenderer::importHostMemory(const PlaneLayout& layout,
    std::uint32_t typeBits, VkDeviceSize size) const
{
    const auto type = m_vctx->findMemoryType(typeBits, 0);
    if (!type)
    {
        akira::log::error("VulkanRenderer: the planes and the decoder backing share no memory type (bits {:#x})",
            typeBits);
        return std::nullopt;
    }

    const VkImportMemoryHostPointerInfoEXT import{
        .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
        .pHostPointer = const_cast<void*>(layout.backing),
    };
    const VkMemoryAllocateInfo allocation{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &import,
        .allocationSize = size,
        .memoryTypeIndex = *type,
    };
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (!succeeded(vkAllocateMemory(m_device, &allocation, nullptr, &memory), "vkAllocateMemory (host import)"))
        return std::nullopt;
    return DeviceMemory(m_device, memory);
}

std::optional<VulkanRenderer::ImagePlanes> VulkanRenderer::importImagePlanes(const PlaneLayout& layout,
    const HostRange& range) const
{
    if (layout.width > m_caps.imageMaxExtent.width || layout.height > m_caps.imageMaxExtent.height)
    {
        akira::log::error("VulkanRenderer: {}x{} exceeds the linear image limit {}x{}", layout.width, layout.height,
            m_caps.imageMaxExtent.width, m_caps.imageMaxExtent.height);
        return std::nullopt;
    }

    const auto createPlane = [this](VkFormat format, std::uint32_t width, std::uint32_t height,
                                 const akira::platform::Nv12Plane& plane) -> std::optional<Image>
    {
        const VkSubresourceLayout subresource{
            .offset = plane.offset,
            .size = 0,
            .rowPitch = plane.pitch,
            .arrayPitch = 0,
            .depthPitch = 0,
        };
        const VkImageDrmFormatModifierExplicitCreateInfoEXT modifier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT,
            .drmFormatModifier = LinearModifier,
            .drmFormatModifierPlaneCount = 1,
            .pPlaneLayouts = &subresource,
        };
        const VkExternalMemoryImageCreateInfo external{
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
            .pNext = &modifier,
            .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
        };
        const VkImageCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = &external,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = { width, height, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        VkImage image = VK_NULL_HANDLE;
        if (!succeeded(vkCreateImage(m_device, &info, nullptr, &image), "vkCreateImage (host plane)"))
            return std::nullopt;
        return Image(m_device, image);
    };

    auto luma = createPlane(VK_FORMAT_R8_UNORM, layout.width, layout.height, layout.luma);
    auto chroma = createPlane(VK_FORMAT_R8G8_UNORM, layout.chromaWidth(), layout.chromaHeight(), layout.chroma);
    if (!luma || !chroma)
        return std::nullopt;

    VkMemoryRequirements lumaRequirements{};
    VkMemoryRequirements chromaRequirements{};
    vkGetImageMemoryRequirements(m_device, luma->get(), &lumaRequirements);
    vkGetImageMemoryRequirements(m_device, chroma->get(), &chromaRequirements);
    if (lumaRequirements.size > range.size || chromaRequirements.size > range.size)
    {
        akira::log::error("VulkanRenderer: linear planes need {:#x}/{:#x} bytes but only {:#x} are importable",
            lumaRequirements.size, chromaRequirements.size, range.size);
        return std::nullopt;
    }

    auto memory = importHostMemory(layout,
        range.typeBits & lumaRequirements.memoryTypeBits & chromaRequirements.memoryTypeBits, range.size);
    if (!memory)
        return std::nullopt;
    if (!succeeded(vkBindImageMemory(m_device, luma->get(), memory->get(), 0), "vkBindImageMemory (luma)") ||
        !succeeded(vkBindImageMemory(m_device, chroma->get(), memory->get(), 0), "vkBindImageMemory (chroma)"))
        return std::nullopt;

    const auto createView = [this](VkImage image, VkFormat format) -> std::optional<ImageView>
    {
        const VkImageViewCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };
        VkImageView view = VK_NULL_HANDLE;
        if (!succeeded(vkCreateImageView(m_device, &info, nullptr, &view), "vkCreateImageView (host plane)"))
            return std::nullopt;
        return ImageView(m_device, view);
    };

    auto lumaView = createView(luma->get(), VK_FORMAT_R8_UNORM);
    auto chromaView = createView(chroma->get(), VK_FORMAT_R8G8_UNORM);
    if (!lumaView || !chromaView)
        return std::nullopt;

    return ImagePlanes{
        .memory = std::move(*memory),
        .luma = std::move(*luma),
        .chroma = std::move(*chroma),
        .lumaView = std::move(*lumaView),
        .chromaView = std::move(*chromaView),
    };
}

std::optional<VulkanRenderer::TexelPlanes> VulkanRenderer::stagingTexelPlanes(const PlaneLayout& layout,
    const HostRange& range) const
{
    const VkBufferCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = range.size,
        .usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer bufferHandle = VK_NULL_HANDLE;
    if (!succeeded(vkCreateBuffer(m_device, &info, nullptr, &bufferHandle), "vkCreateBuffer (host planes)"))
        return std::nullopt;
    Buffer buffer(m_device, bufferHandle);

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(m_device, buffer.get(), &requirements);
    const auto type = m_vctx->findMemoryType(requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!type)
    {
        akira::log::error("VulkanRenderer: no host-visible coherent memory type can back the plane buffer");
        return std::nullopt;
    }
    const VkMemoryAllocateInfo allocation{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = *type,
    };
    VkDeviceMemory memoryHandle = VK_NULL_HANDLE;
    if (!succeeded(vkAllocateMemory(m_device, &allocation, nullptr, &memoryHandle), "vkAllocateMemory (plane copy)"))
        return std::nullopt;
    DeviceMemory memory(m_device, memoryHandle);
    if (!succeeded(vkBindBufferMemory(m_device, buffer.get(), memory.get(), 0), "vkBindBufferMemory (plane copy)"))
        return std::nullopt;
    void* mapped = nullptr;
    if (!succeeded(vkMapMemory(m_device, memory.get(), 0, VK_WHOLE_SIZE, 0, &mapped), "vkMapMemory (plane copy)"))
        return std::nullopt;

    auto planes = texelViews(layout, buffer.get());
    if (!planes)
        return std::nullopt;
    planes->memory = std::move(memory);
    planes->buffer = std::move(buffer);
    planes->mapped = mapped;
    planes->size = static_cast<std::size_t>(range.size);
    return planes;
}

std::optional<VulkanRenderer::TexelPlanes> VulkanRenderer::texelViews(const PlaneLayout& layout, VkBuffer buffer) const
{
    const VkDeviceSize lumaAlignment = m_caps.texelOffsetAlignment;
    const VkDeviceSize chromaAlignment = std::lcm(m_caps.texelOffsetAlignment, VkDeviceSize{2});
    const VkDeviceSize lumaOffset = layout.luma.offset / lumaAlignment * lumaAlignment;
    const VkDeviceSize chromaOffset = layout.chroma.offset / chromaAlignment * chromaAlignment;
    const VkDeviceSize lumaRange = layout.lumaEnd() - lumaOffset;
    const VkDeviceSize chromaRange = layout.chromaEnd() - chromaOffset;

    if (lumaRange > m_caps.maxTexelBufferElements || chromaRange / 2 > m_caps.maxTexelBufferElements)
    {
        akira::log::error("VulkanRenderer: plane spans {}/{} texels, beyond maxTexelBufferElements {}",
            lumaRange, chromaRange / 2, m_caps.maxTexelBufferElements);
        return std::nullopt;
    }

    const auto createView = [this, buffer](VkFormat format, VkDeviceSize offset,
                                VkDeviceSize size) -> std::optional<BufferView>
    {
        const VkBufferViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
            .buffer = buffer,
            .format = format,
            .offset = offset,
            .range = size,
        };
        VkBufferView view = VK_NULL_HANDLE;
        if (!succeeded(vkCreateBufferView(m_device, &viewInfo, nullptr, &view), "vkCreateBufferView (host plane)"))
            return std::nullopt;
        return BufferView(m_device, view);
    };

    auto lumaView = createView(VK_FORMAT_R8_UNORM, lumaOffset, lumaRange);
    auto chromaView = createView(VK_FORMAT_R8G8_UNORM, chromaOffset, chromaRange);
    if (!lumaView || !chromaView)
        return std::nullopt;

    return TexelPlanes{
        .luma = std::move(*lumaView),
        .chroma = std::move(*chromaView),
        .bases = {
            static_cast<std::uint32_t>(layout.luma.offset - lumaOffset),
            layout.luma.pitch,
            static_cast<std::uint32_t>((layout.chroma.offset - chromaOffset) / 2),
            layout.chroma.pitch / 2,
        },
    };
}

std::unique_ptr<VulkanRenderer::PlaneImport> VulkanRenderer::importPlanes(const PlaneLayout& layout,
    const HostRange& range, ImportCache& cache) const
{
    if (cache.path() == PlanePath::Image)
    {
        auto planes = importImagePlanes(layout, range);
        if (!planes)
            return nullptr;
        const auto set = allocateSet(cache.pool(), m_shared.imageSetLayout.get());
        if (!set)
            return nullptr;

        const std::array descriptors{
            VkDescriptorImageInfo{ m_shared.sampler.get(), planes->lumaView.get(), VK_IMAGE_LAYOUT_GENERAL },
            VkDescriptorImageInfo{ m_shared.sampler.get(), planes->chromaView.get(), VK_IMAGE_LAYOUT_GENERAL },
        };
        const VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = *set,
            .dstBinding = 0,
            .descriptorCount = static_cast<std::uint32_t>(descriptors.size()),
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = descriptors.data(),
        };
        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
        return std::make_unique<PlaneImport>(layout, std::move(*planes), *set);
    }

    if (layout.chroma.offset % 2 != 0 || layout.chroma.pitch % 2 != 0)
    {
        akira::log::error("VulkanRenderer: chroma plane offset {:#x} / pitch {} is not RG8 addressable",
            layout.chroma.offset, layout.chroma.pitch);
        return nullptr;
    }
    auto planes = layout.decoderBuffer ? texelViews(layout, layout.decoderBuffer) : stagingTexelPlanes(layout, range);
    if (!planes)
        return nullptr;
    const auto set = allocateSet(cache.pool(), m_shared.texelSetLayout.get());
    if (!set)
        return nullptr;

    const std::array views{ planes->luma.get(), planes->chroma.get() };
    const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = *set,
        .dstBinding = 0,
        .descriptorCount = static_cast<std::uint32_t>(views.size()),
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
        .pTexelBufferView = views.data(),
    };
    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    return std::make_unique<PlaneImport>(layout, std::move(*planes), *set);
}

std::shared_ptr<VulkanRenderer::ImportCache> VulkanRenderer::createImportCache(std::uint64_t generation) const
{
    constexpr std::uint32_t slots = akira::platform::Nv12Frame::SlotCount;
    auto pool = createDescriptorPool(m_path == PlanePath::Image ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                                                                : VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
        slots, slots * 2);
    if (!pool)
        return nullptr;
    akira::log::info("VulkanRenderer: importing decoder generation {} through {}", generation,
        m_path == PlanePath::Image ? "linear images" : "texel buffers");
    return std::make_shared<ImportCache>(generation, m_path, std::move(*pool));
}

VulkanRenderer::PlaneImport* VulkanRenderer::acquireImport(const Frame& frame)
{
    PlaneLayout layout = planeLayout(*frame);
    if (layout.decoderBuffer && m_decoder_sampling == DecoderSampling::Unchecked)
    {
        m_decoder_sampling = decoderSamplingMatches(layout) ? DecoderSampling::Verified : DecoderSampling::Broken;
        if (m_decoder_sampling == DecoderSampling::Verified)
            akira::log::info("VulkanRenderer: the GPU reads what the decoder wrote, frames are sampled in place");
        else
        {
            akira::log::warning("VulkanRenderer: the GPU does not read what the decoder wrote, frames are copied");
            layout = planeLayout(*frame);
        }
    }
    const std::uint32_t slot = frame->slot();
    if (slot >= akira::platform::Nv12Frame::SlotCount)
    {
        akira::log::error("VulkanRenderer: decoder slot {} is out of range", slot);
        return nullptr;
    }

    bool stale = !m_cache || m_cache->generation() != frame->generation() || m_cache->path() != m_path;
    if (!stale)
    {
        if (const PlaneImport* existing = m_cache->find(slot); existing && !(existing->layout() == layout))
        {
            akira::log::warning("VulkanRenderer: slot {} changed its plane layout, re-importing generation {}",
                slot, frame->generation());
            stale = true;
        }
    }

    Frame previous;
    {
        std::lock_guard lock(m_frame_mutex);
        previous = std::exchange(m_last_frame, frame);
    }

    if (stale)
    {
        if (m_cache)
            retire(Retired{ .frame = std::move(previous), .cache = std::move(m_cache) });
        m_cache = createImportCache(frame->generation());
        if (!m_cache)
            return nullptr;
    }

    if (PlaneImport* existing = m_cache->find(slot))
    {
        existing->copyFrom(layout.backing);
        return existing;
    }
    if (m_cache->failed(slot))
        return nullptr;

    const auto range = hostRange(layout);
    if (!range)
    {
        m_cache->markFailed(slot);
        return nullptr;
    }

    auto created = importPlanes(layout, *range, *m_cache);
    if (!created && layout.decoderBuffer)
    {
        akira::log::warning("VulkanRenderer: slot {} of generation {} cannot be sampled in place, frames are copied",
            slot, frame->generation());
        m_decoder_sampling = DecoderSampling::Broken;
        return acquireImport(frame);
    }
    if (!created)
    {
        m_cache->markFailed(slot);
        if (m_path == PlanePath::Image && m_caps.texelPlanes)
        {
            akira::log::warning("VulkanRenderer: linear image import of slot {} failed, switching to texel buffer "
                "planes", slot);
            m_path = PlanePath::TexelBuffer;
            return acquireImport(frame);
        }
        akira::log::error("VulkanRenderer: slot {} of generation {} cannot be sampled in place, its frames are "
            "dropped", slot, frame->generation());
        return nullptr;
    }
    PlaneImport& stored = m_cache->store(slot, std::move(created));
    stored.copyFrom(layout.backing);
    return &stored;
}

VkPipelineLayout VulkanRenderer::layoutFor(const PipelineKey& key) const
{
    if (key.program == Program::Video || key.program == Program::Easu)
        return key.path == PlanePath::Image ? m_shared.imagePipelineLayout.get() : m_shared.texelPipelineLayout.get();
    return m_shared.texturePipelineLayout.get();
}

VkPipeline VulkanRenderer::pipeline(const PipelineKey& key)
{
    if (const auto found = m_shared.pipelines.find(key); found != m_shared.pipelines.end())
        return found->second.get();

    auto created = createPipeline(key);
    return m_shared.pipelines.emplace(key, created ? std::move(*created) : Pipeline{}).first->second.get();
}

std::optional<VulkanRenderer::Pipeline> VulkanRenderer::createPipeline(const PipelineKey& key) const
{
    const bool image = key.path == PlanePath::Image;
    const Shader fragment = [&]
    {
        switch (key.program)
        {
            case Program::Video: return image ? Shader::VideoImage : Shader::VideoTexel;
            case Program::Easu: return image ? Shader::EasuImage : Shader::EasuTexel;
            case Program::Rcas: return Shader::Rcas;
            case Program::Pass: return Shader::Pass;
            case Program::Overlay: return Shader::Overlay;
            case Program::Border: return Shader::Border;
        }
        return Shader::Pass;
    }();
    const Shader vertex = key.program == Program::Overlay ? Shader::OverlayVertex : Shader::FullscreenVertex;
    const bool specialized = key.program == Program::Video || key.program == Program::Easu;
    const bool blended = key.program == Program::Overlay || key.program == Program::Border;

    const std::array<std::uint32_t, 2> constants{
        key.dithering ? VK_TRUE : VK_FALSE,
        std::bit_cast<std::uint32_t>(SettingsManager::getInstance()->getDitheringStrength()),
    };
    const std::array entries{
        VkSpecializationMapEntry{ 0, 0, sizeof(std::uint32_t) },
        VkSpecializationMapEntry{ 1, sizeof(std::uint32_t), sizeof(std::uint32_t) },
    };
    const VkSpecializationInfo specialization{
        .mapEntryCount = static_cast<std::uint32_t>(entries.size()),
        .pMapEntries = entries.data(),
        .dataSize = sizeof(constants),
        .pData = constants.data(),
    };

    const std::array stages{
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = m_shared.shaders[static_cast<std::size_t>(vertex)].get(),
            .pName = "main",
        },
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = m_shared.shaders[static_cast<std::size_t>(fragment)].get(),
            .pName = "main",
            .pSpecializationInfo = specialized ? &specialization : nullptr,
        },
    };

    const VkPipelineVertexInputStateCreateInfo vertexInput{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
    const VkPipelineInputAssemblyStateCreateInfo assembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = key.program == Program::Overlay ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
                                                    : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    const VkPipelineViewportStateCreateInfo viewport{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    const VkPipelineRasterizationStateCreateInfo rasterization{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
    };
    const VkPipelineMultisampleStateCreateInfo multisample{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    const VkPipelineDepthStencilStateCreateInfo depthStencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };
    const VkPipelineColorBlendAttachmentState attachment{
        .blendEnable = blended ? VK_TRUE : VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT,
    };
    const VkPipelineColorBlendStateCreateInfo colorBlend{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &attachment,
    };
    const std::array dynamics{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    const VkPipelineDynamicStateCreateInfo dynamic{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<std::uint32_t>(dynamics.size()),
        .pDynamicStates = dynamics.data(),
    };

    const bool swapchain = key.target == Target::Swapchain;
    const VkFormat colorFormat = swapchain ? m_vctx->colorFormat() : OffscreenFormat;
    const VkPipelineRenderingCreateInfo rendering{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat,
        .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = swapchain ? m_vctx->stencilFormat() : VK_FORMAT_UNDEFINED,
    };

    const VkGraphicsPipelineCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering,
        .stageCount = static_cast<std::uint32_t>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &assembly,
        .pViewportState = &viewport,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlend,
        .pDynamicState = &dynamic,
        .layout = layoutFor(key),
    };
    VkPipeline handle = VK_NULL_HANDLE;
    if (!succeeded(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &info, nullptr, &handle),
            "vkCreateGraphicsPipelines"))
        return std::nullopt;
    return Pipeline(m_device, handle);
}

std::shared_ptr<VulkanRenderer::FsrState> VulkanRenderer::createFsr(const FsrConfig& config) const
{
    if (!config.easu && !config.rcas)
        return nullptr;

    akira::log::info("VulkanRenderer::createFsr: input={}x{} target={}x{} display={}x{} ss={} ratio={:.2f}x{:.2f}",
        config.inputWidth, config.inputHeight, config.targetWidth, config.targetHeight, config.display.width,
        config.display.height, config.supersampling,
        static_cast<float>(config.targetWidth) / static_cast<float>(config.inputWidth),
        static_cast<float>(config.targetHeight) / static_cast<float>(config.inputHeight));

    auto pool = createDescriptorPool(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, 2);
    if (!pool)
        return nullptr;

    const VkExtent2D target{ static_cast<std::uint32_t>(config.targetWidth),
        static_cast<std::uint32_t>(config.targetHeight) };
    constexpr VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    auto easu = createRenderTarget(target, OffscreenFormat, usage, VK_IMAGE_ASPECT_COLOR_BIT, pool->get());
    if (!easu)
        return nullptr;

    std::optional<RenderTarget> rcas;
    if (config.rcasTarget)
    {
        rcas = createRenderTarget(target, OffscreenFormat, usage, VK_IMAGE_ASPECT_COLOR_BIT, pool->get());
        if (!rcas)
            return nullptr;
    }

    akira::log::info("VulkanRenderer::createFsr: initialized, sharpness={}", config.sharpness);
    return std::make_shared<FsrState>(config, std::move(*pool), std::move(*easu), std::move(rcas));
}

void VulkanRenderer::publishFsrSummary(const FsrState* fsr)
{
    FsrSummary summary;
    if (fsr)
    {
        const FsrConfig& config = fsr->config();
        summary = FsrSummary{
            .active = true,
            .easu = config.easu,
            .rcas = config.rcas,
            .supersampling = config.supersampling,
            .sharpness = config.sharpness,
            .inputHeight = config.inputHeight,
            .targetHeight = config.targetHeight,
            .displayHeight = static_cast<int>(config.display.height),
        };
    }
    std::lock_guard lock(m_fsr_summary_mutex);
    m_fsr_summary = summary;
}

void VulkanRenderer::retire(Retired retired)
{
    m_vctx->deferUntilFrameRetired([retired = std::move(retired)] {});
}

void VulkanRenderer::setConnectionContext(const std::string& context)
{
    m_connection_context = context;
    m_ovl_dirty = true;
}

void VulkanRenderer::setDithering(bool enabled)
{
    if (m_dithering_enabled.exchange(enabled, std::memory_order_relaxed) != enabled)
        akira::log::info("VulkanRenderer: switching dithering to {}", enabled);
}

void VulkanRenderer::setStatsOverlayMode(StatsOverlayMode mode)
{
    const StatsOverlayMode previous = m_stats_mode.exchange(mode, std::memory_order_relaxed);
    if (mode == StatsOverlayMode::Off)
    {
        std::lock_guard lock(m_ovl_mutex);
        m_ovl_front = -1;
    }
    else if (previous != mode)
        m_ovl_dirty = true;
}

void VulkanRenderer::draw(Frame frame)
{
    if (!m_initialized.load(std::memory_order_acquire) || !frame)
        return;

    if (m_paused.load(std::memory_order_relaxed))
        return;

    std::lock_guard lock(m_frame_mutex);
    m_current_frame = std::move(frame);
    m_frame_bound.store(true, std::memory_order_release);
}

void VulkanRenderer::presentFrame(Frame frame)
{
    draw(std::move(frame));

    if (!m_frame_bound.load(std::memory_order_acquire) || m_paused.load(std::memory_order_relaxed))
        return;

    VideoContext* videoContext = brls::Application::getPlatform()->getVideoContext();
    videoContext->beginFrame();
    renderVideo();
    videoContext->endFrame();

    recordPresentedFrame();
}

void VulkanRenderer::recordPresentedFrame()
{
    const auto now = std::chrono::steady_clock::now();
    if (!m_render_fps_init)
    {
        m_render_fps_start = now;
        m_render_frame_count = 0;
        m_render_fps_init = true;
    }

    m_render_frame_count++;
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_render_fps_start).count();
    if (elapsed >= 1000)
    {
        m_render_fps.store(static_cast<float>(m_render_frame_count * 1000) / static_cast<float>(elapsed),
            std::memory_order_relaxed);
        m_render_frame_count = 0;
        m_render_fps_start = now;
    }
}

bool VulkanRenderer::ensureSwapchainRendering()
{
    if (!m_vctx->isRendering())
        m_vctx->beginRendering(VK_ATTACHMENT_LOAD_OP_LOAD);
    return m_vctx->isRendering();
}

void VulkanRenderer::renderVideo()
{
    std::lock_guard renderLock(m_render_mutex);
    if (!m_initialized.load(std::memory_order_acquire) || !m_frame_bound.load(std::memory_order_acquire))
        return;

    Frame frame;
    {
        std::lock_guard lock(m_frame_mutex);
        frame = std::move(m_current_frame);
    }
    if (!frame)
        return;

    const int width = static_cast<int>(frame->width());
    const int height = static_cast<int>(frame->height());
    if (width != m_configured_width || height != m_configured_height)
    {
        akira::log::info("VulkanRenderer: frame size {}x{} -> {}x{}", m_configured_width, m_configured_height,
            width, height);
        if (m_fsr)
            retire(Retired{ .fsr = std::move(m_fsr) });
        auto* settings = SettingsManager::getInstance();
        const int targetH = settings->getEasuTargetHeight();
        const int targetW = (targetH * 16) / 9;
        const bool easuNeeded = settings->getEasuEnabled() && targetH > 0 && (width < targetW || height < targetH);
        m_fsr_pending = easuNeeded || settings->getRcasEnabled();
        m_configured_width = width;
        m_configured_height = height;
        publishFsrSummary(nullptr);
    }

    const VkExtent2D extent = m_vctx->frameExtent();
    if (m_fsr && (m_fsr->config().display.width != extent.width || m_fsr->config().display.height != extent.height))
    {
        retire(Retired{ .fsr = std::move(m_fsr) });
        m_fsr_pending = true;
    }
    if (m_fsr_pending)
    {
        m_fsr = createFsr(fsrConfig(width, height, extent));
        m_fsr_pending = false;
        publishFsrSummary(m_fsr.get());
    }

    {
        const std::int64_t timestamp = frame->timestampNs();
        if (timestamp < m_drawn_timestamp)
        {
            akira::log::warning("VulkanRenderer: drawing frame {} (slot {}) after frame {} (slot {}), {} us back in time",
                timestamp, frame->slot(), m_drawn_timestamp, m_drawn_slot, (m_drawn_timestamp - timestamp) / 1000);
        }
        m_drawn_timestamp = timestamp;
        m_drawn_slot = frame->slot();
    }
    PlaneImport* import = acquireImport(frame);
    if (!import)
        return;
    recordAcquire(*import);

    Retired retired{ .frame = frame, .cache = m_cache, .fsr = m_fsr };
    const PlaneLayout& layout = import->layout();
    const VideoPush push{
        .planes = import->bases(),
        .size = { layout.width, layout.height, layout.chromaWidth(), layout.chromaHeight() },
    };

    VkCommandBuffer cmd = m_vctx->frameCommandBuffer();
    if (m_fsr)
        drawVideoFsr(cmd, *import, *m_fsr, push);
    else if (ensureSwapchainRendering())
        drawPass(cmd,
            PipelineKey{ .program = Program::Video, .target = Target::Swapchain, .path = m_cache->path(),
                .dithering = m_dithering_enabled.load(std::memory_order_relaxed) },
            extent, import->descriptorSet(), std::as_bytes(std::span(&push, 1)));

    retired.overlay = compositeOverlay(cmd);

    if (m_border_flash_frames.load(std::memory_order_acquire) > 0)
        renderBorderFlash(cmd);

    retire(std::move(retired));
}

void VulkanRenderer::recordAcquire(PlaneImport& import)
{
    std::vector<VkImageMemoryBarrier2> images;
    import.appendAcquireBarriers(images);

    const VkCommandBuffer upload = m_vctx->uploadCommandBuffer();
    const bool fromDecoder = import.layout().decoderBuffer != VK_NULL_HANDLE;
    const VkMemoryBarrier2 memory{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
    };
    const VkBufferMemoryBarrier2 decoderWrite{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
        .dstQueueFamilyIndex = m_vctx->queueFamilyIndex(),
        .buffer = import.layout().decoderBuffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE,
    };
    const VkDependencyInfo acquire{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &memory,
        .bufferMemoryBarrierCount = fromDecoder ? 1u : 0u,
        .pBufferMemoryBarriers = fromDecoder ? &decoderWrite : nullptr,
        .imageMemoryBarrierCount = static_cast<std::uint32_t>(images.size()),
        .pImageMemoryBarriers = images.data(),
    };
    vkCmdPipelineBarrier2(upload, &acquire);
}

void VulkanRenderer::drawPass(VkCommandBuffer cmd, const PipelineKey& key, VkExtent2D extent, VkDescriptorSet set,
    std::span<const std::byte> push)
{
    const VkPipeline handle = pipeline(key);
    if (handle == VK_NULL_HANDLE)
        return;

    const VkPipelineLayout layout = layoutFor(key);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, handle);
    setViewport(cmd, extent);
    if (set != VK_NULL_HANDLE)
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0, nullptr);
    if (!push.empty())
        vkCmdPushConstants(cmd, layout, pushStages(key.program), 0, static_cast<std::uint32_t>(push.size()),
            push.data());
    vkCmdDraw(cmd, key.program == Program::Overlay ? 4 : 3, 1, 0, 0);
}

void VulkanRenderer::renderOffscreen(VkCommandBuffer cmd, const RenderTarget& target, const PipelineKey& key,
    VkDescriptorSet set, std::span<const std::byte> push)
{
    imageBarrier(cmd, target.image(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    const VkRenderingAttachmentInfo color{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = target.view(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };
    const VkRenderingInfo info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { { 0, 0 }, target.extent() },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color,
    };
    vkCmdBeginRendering(cmd, &info);
    drawPass(cmd, key, target.extent(), set, push);
    vkCmdEndRendering(cmd);

    imageBarrier(cmd, target.image(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}

void VulkanRenderer::drawVideoFsr(VkCommandBuffer cmd, const PlaneImport& import, const FsrState& fsr,
    VideoPush push)
{
    const FsrConfig& config = fsr.config();
    const VkExtent2D extent = m_vctx->frameExtent();
    const PlanePath path = m_cache->path();
    push.easu = config.easuConstants;

    m_vctx->endRendering();

    const RenderTarget& easu = fsr.easuTarget();
    const PipelineKey first = config.easu
        ? PipelineKey{ .program = Program::Easu, .target = Target::Offscreen, .path = path,
              .dithering = config.dithering }
        : PipelineKey{ .program = Program::Video, .target = Target::Offscreen, .path = path,
              .dithering = m_dithering_enabled.load(std::memory_order_relaxed) };
    renderOffscreen(cmd, easu, first, import.descriptorSet(), std::as_bytes(std::span(&push, 1)));

    const auto rcasPush = std::as_bytes(std::span(config.rcasConstants));
    if (const RenderTarget* rcas = fsr.rcasTarget())
    {
        renderOffscreen(cmd, *rcas, PipelineKey{ .program = Program::Rcas, .target = Target::Offscreen },
            easu.descriptorSet(), rcasPush);
        if (!ensureSwapchainRendering())
            return;
        drawPass(cmd, PipelineKey{ .program = Program::Pass, .target = Target::Swapchain }, extent,
            rcas->descriptorSet(), {});
        return;
    }

    if (!ensureSwapchainRendering())
        return;
    if (config.rcas)
        drawPass(cmd, PipelineKey{ .program = Program::Rcas, .target = Target::Swapchain }, extent,
            easu.descriptorSet(), rcasPush);
    else
        drawPass(cmd, PipelineKey{ .program = Program::Pass, .target = Target::Swapchain }, extent,
            easu.descriptorSet(), {});
}

std::shared_ptr<VulkanRenderer::OverlayTarget> VulkanRenderer::compositeOverlay(VkCommandBuffer cmd)
{
    std::shared_ptr<OverlayTarget> target;
    int front = -1;
    float panelW = 0.0f;
    float panelH = 0.0f;
    {
        std::lock_guard lock(m_ovl_mutex);
        target = m_ovl_target;
        front = m_ovl_front;
        panelW = m_ovl_panel_w;
        panelH = m_ovl_panel_h;
    }
    if (!target || front < 0)
        return nullptr;

    const float sw = static_cast<float>(brls::Application::windowWidth);
    const float sh = static_cast<float>(brls::Application::windowHeight);
    if (sw <= 0.0f || sh <= 0.0f || panelW <= 0.0f || panelH <= 0.0f)
        return nullptr;

    const float ox = m_overlay_x.load(std::memory_order_relaxed);
    const float oy = m_overlay_y.load(std::memory_order_relaxed);
    const VkExtent2D rt = target->extent();
    const QuadPush push{
        .first = {
            (ox / sw) * 2.0f - 1.0f,
            (oy / sh) * 2.0f - 1.0f,
            ((ox + panelW) / sw) * 2.0f - 1.0f,
            ((oy + panelH) / sh) * 2.0f - 1.0f,
        },
        .second = { panelW / static_cast<float>(rt.width), panelH / static_cast<float>(rt.height), 0.0f, 0.0f },
    };

    if (!ensureSwapchainRendering())
        return nullptr;
    drawPass(cmd, PipelineKey{ .program = Program::Overlay, .target = Target::Swapchain }, m_vctx->frameExtent(),
        target->color(front).descriptorSet(), std::as_bytes(std::span(&push, 1)));
    return target;
}

void VulkanRenderer::renderBorderFlash(VkCommandBuffer cmd)
{
    int before = m_border_flash_frames.load(std::memory_order_acquire);
    while (before > 0
           && !m_border_flash_frames.compare_exchange_weak(
               before, before - 1, std::memory_order_acq_rel)) {
    }
    if (before <= 0)
        return;
    const int remaining = before - 1;

    const float sw = static_cast<float>(brls::Application::windowWidth);
    const float sh = static_cast<float>(brls::Application::windowHeight);
    if (sw <= 0.0f || sh <= 0.0f)
        return;

    const float fade = static_cast<float>(remaining) / static_cast<float>(BORDER_FLASH_DURATION);
    const float alpha = fade * 0.25f;
    if (alpha <= 0.002f)
        return;

    const NVGcolor accent = ovl(app::views::active().accent, 255);
    const QuadPush push{
        .first = { accent.r, accent.g, accent.b, alpha },
        .second = { sw, sh, 64.0f, 0.0f },
    };

    if (!ensureSwapchainRendering())
        return;
    drawPass(cmd, PipelineKey{ .program = Program::Border, .target = Target::Swapchain }, m_vctx->frameExtent(),
        VK_NULL_HANDLE, std::as_bytes(std::span(&push, 1)));
}

void VulkanRenderer::registerCallback()
{
    if (m_callback_registered)
        return;

    brls::Application::setPostRenderCallback([this]() {
        if (m_tick_callback)
            m_tick_callback();

        if (m_frame_bound.load(std::memory_order_acquire) && !m_paused.load(std::memory_order_relaxed))
        {
            renderVideo();
        }
    });

    m_callback_registered = true;
    akira::log::info("VulkanRenderer: post-render callback registered");
}

void VulkanRenderer::unregisterCallback()
{
    if (!m_callback_registered)
        return;

    brls::Application::setExclusiveRender(false);
    brls::Application::setPostRenderCallback(nullptr);
    m_callback_registered = false;
    akira::log::info("VulkanRenderer: post-render callback unregistered");
}

void VulkanRenderer::cleanup()
{
    if (!m_initialized.exchange(false, std::memory_order_acq_rel))
        return;

    akira::log::info("VulkanRenderer::cleanup");

    unregisterCallback();

    std::lock_guard renderLock(m_render_mutex);
    Retired retired{ .fsr = std::move(m_fsr) };
    {
        std::lock_guard lock(m_ovl_mutex);
        retired.overlay = std::move(m_ovl_target);
        m_ovl_front = -1;
    }
    {
        std::lock_guard lock(m_frame_mutex);
        m_current_frame.reset();
        retired.frame = std::move(m_last_frame);
        retired.cache = std::move(m_cache);
    }
    m_frame_bound.store(false, std::memory_order_release);
    m_fsr_pending = false;
    m_configured_width = 0;
    m_configured_height = 0;
    publishFsrSummary(nullptr);
    destroyOverlayContext();

    retire(std::move(retired));
    m_vctx->deferUntilFrameRetired([shared = std::make_shared<SharedObjects>(std::move(m_shared))] {});
    m_shared = SharedObjects{};
}

void VulkanRenderer::waitIdle()
{
    if (!m_vctx)
        return;

    std::lock_guard lock(m_vctx->queueMutex());
    succeeded(vkQueueWaitIdle(m_vctx->queue()), "vkQueueWaitIdle");
}

void VulkanRenderer::warmFontAtlas(NVGcontext* vg)
{
    if (m_font_atlas_warm)
        return;

    static const char* charset =
        "0123456789.,:/%+-x ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz·×›";

    nvgFontFaceId(vg, m_ovl_context->font());
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 0));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

    for (float size : { 9.5f, 10.0f, 10.5f, 11.5f, 12.0f, 12.5f, 13.0f, 14.0f })
    {
        nvgFontSize(vg, ovlSize(size * m_overlay_scale));
        nvgText(vg, -4096.0f, -4096.0f, charset, nullptr);
    }

    m_font_atlas_warm = true;
}

void VulkanRenderer::rebuildOverlayText()
{
    const StreamStats m_stats = readStreamStats();
    FsrSummary fsr;
    {
        std::lock_guard lock(m_fsr_summary_mutex);
        fsr = m_fsr_summary;
    }

    uint64_t mins = m_stats.stream_duration_seconds / 60;
    uint64_t secs = m_stats.stream_duration_seconds % 60;

    auto& wg = WireGuardManager::instance();

    m_ov.fps = std::format("{:.1f}", m_render_fps.load(std::memory_order_relaxed));
    m_ov.rate = std::format("{:.1f}", m_stats.measured_bitrate_mbps);
    m_ov.loss = std::format("{:.1f}", m_stats.packet_loss_percent);
    m_ov.rtt = m_stats.rtt_valid ? std::format("{:.0f}", m_stats.rtt_ms) : std::string("--");
    m_ov.uptime = std::format("{}m{:02}s", mins, secs);

    m_ov.req_res = std::format("{}x{}", m_stats.requested_width, m_stats.requested_height);
    m_ov.req_fps = std::format("{}", m_stats.requested_fps);
    m_ov.req_rate = std::format("{:.1f}", m_stats.requested_bitrate / 1000.0f);
    m_ov.req_codec = m_stats.requested_hevc ? "HEVC" : "H.264";

    m_ov.out_res = std::format("{}x{}", m_stats.video_width, m_stats.video_height);
    m_ov.out_codec = m_stats.is_hevc ? "HEVC" : "H.264";
    m_ov.out_path = m_stats.is_hardware_decoder ? "HW" : "SW";

    m_ov.fsr_active = fsr.active;
    m_ov.fsr_path.clear();
    m_ov.fsr_detail.clear();
    m_ov.fsr_compact.clear();
    if (m_ov.fsr_active)
    {
        if (fsr.easu)
        {
            m_ov.fsr_path = std::format("{}›{}›{}",
                fsr.inputHeight, fsr.targetHeight, fsr.displayHeight);
            const float scale = fsr.inputHeight > 0
                ? static_cast<float>(fsr.targetHeight) / static_cast<float>(fsr.inputHeight)
                : 1.0f;
            m_ov.fsr_detail = "EASU";
            m_ov.fsr_compact = std::format("{:.2f}× {}", scale,
                fsr.supersampling ? "SS" : "EASU");
        }
        else
        {
            m_ov.fsr_path = std::format("{}›{}", fsr.inputHeight, fsr.displayHeight);
            m_ov.fsr_compact = "RCAS";
        }

        if (fsr.rcas)
        {
            const int strength = std::clamp(static_cast<int>(std::lround(
                (1.0f - fsr.sharpness / 2.0f) * 100.0f)), 0, 100);
            const std::string rcas = std::format("RCAS {}%", strength);
            if (!m_ov.fsr_detail.empty())
                m_ov.fsr_detail += " · ";
            m_ov.fsr_detail += rcas;
            if (fsr.easu)
                m_ov.fsr_compact += std::format(" · {}", rcas);
            else
                m_ov.fsr_compact = rcas;
        }
    }

    m_ov.lost = std::format("{}", m_stats.network_frames_lost);
    m_ov.recovered = std::format("{}", m_stats.frames_recovered);

    m_ov.ghash = "PMULL";
    m_ov.vpn = wg.isConnected() ? wg.getTunnelIP() : "Off";

    m_ov.context = m_connection_context.empty()
        ? std::format("{} \xc2\xb7 {}",
            m_stats.is_hevc ? "PS5" : "PS4",
            wg.isConnected() ? "VPN" : "Direct")
        : m_connection_context;

    m_ov.lat_valid = m_stats.latency_valid;
    m_ov.lat_net_ms = m_stats.net_ms;
    m_ov.lat_visual_ms = m_stats.visual_ms;
    m_ov.lat_total_ms = m_stats.total_ms;

    m_ov.lat_net = m_stats.latency_valid ? std::format("{:.0f}", m_stats.net_ms) : std::string("--");
    m_ov.lat_visual = std::format("{:.1f}", m_stats.visual_ms);
    m_ov.lat_total = m_stats.latency_valid ? std::format("{:.0f}", m_stats.total_ms) : std::string("--");
    m_ov.lat_jitter = std::format("{:.1f}", m_stats.jitter_ms);
    m_ov.lat_decode = std::format("{:.1f}", m_stats.decode_ms);
    m_ov.src_fps = std::format("{:.1f}", m_stats.source_fps);
}

VulkanRenderer::Tone VulkanRenderer::toneFps() const
{
    if (m_stats.requested_fps <= 0)
        return Tone::Good;
    float r = m_render_fps.load(std::memory_order_relaxed) / (float)m_stats.requested_fps;
    return r >= 0.92f ? Tone::Good : (r >= 0.75f ? Tone::Warn : Tone::Bad);
}

VulkanRenderer::Tone VulkanRenderer::toneLoss() const
{
    float l = m_stats.packet_loss_percent;
    return l < 0.5f ? Tone::Good : (l < 2.0f ? Tone::Warn : Tone::Bad);
}

VulkanRenderer::Tone VulkanRenderer::toneRate() const
{
    if (m_stats.requested_bitrate <= 0)
        return Tone::Good;
    float r = m_stats.measured_bitrate_mbps / (m_stats.requested_bitrate / 1000.0f);
    return r >= 0.7f ? Tone::Good : (r >= 0.4f ? Tone::Warn : Tone::Bad);
}

VulkanRenderer::Tone VulkanRenderer::toneRtt() const
{
    if (!m_stats.rtt_valid)
        return Tone::Good;
    float v = m_stats.rtt_ms;
    return v < 40.0f ? Tone::Good : (v < 80.0f ? Tone::Warn : Tone::Bad);
}

VulkanRenderer::Tone VulkanRenderer::toneLost() const
{
    size_t v = m_stats.network_frames_lost;
    return v == 0 ? Tone::Good : (v <= 20 ? Tone::Warn : Tone::Bad);
}

VulkanRenderer::Tone VulkanRenderer::toneLatency() const
{
    if (!m_stats.latency_valid)
        return Tone::Good;
    float v = m_stats.total_ms;
    return v < 50.0f ? Tone::Good : (v < 100.0f ? Tone::Warn : Tone::Bad);
}

NVGcolor VulkanRenderer::toneColor(Tone tone) const
{
    switch (tone)
    {
        case Tone::Warn: return app::views::active().warning;
        case Tone::Bad:  return app::views::active().danger;
        default:         return ovl(app::views::active().text, 242);
    }
}

void VulkanRenderer::drawOverlayPanel(NVGcontext* vg, float x, float y, float w, float h)
{
    float s = m_overlay_scale;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, 10.0f * s);
    nvgFillColor(vg, ovl(app::views::active().backgroundDeep, 148));
    nvgFill(vg);
    nvgStrokeWidth(vg, 1.0f);
    nvgStrokeColor(vg, ovl(app::views::active().text, 38));
    nvgStroke(vg);
}

void VulkanRenderer::drawLatencyStrip(NVGcontext* vg, float x, float y, float w, float h)
{
    float s = m_overlay_scale;
    const app::views::Palette& p = app::views::active();
    (void)h;

    nvgFontSize(vg, ovlSize(9.5f * s));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, ovl(p.text, 87));
    nvgText(vg, x, y + 9.0f * s, "LATENCY", nullptr);

    nvgFontSize(vg, ovlSize(10.0f * s));
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, ovl(p.text, 117));
    nvgText(vg, x + w, y + 9.0f * s, "ms", nullptr);
    float tright = x + w - nvgTextBounds(vg, 0, 0, "ms", nullptr, nullptr) - 3.0f * s;

    nvgFontSize(vg, ovlSize(13.0f * s));
    nvgFillColor(vg, toneColor(toneLatency()));
    nvgText(vg, tright, y + 9.0f * s, m_ov.lat_total.c_str(), nullptr);

    float barY = y + 19.0f * s;
    float barH = 7.0f * s;
    float radius = barH * 0.5f;

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, barY, w, barH, radius);
    nvgFillColor(vg, ovl(p.text, 20));
    nvgFill(vg);

    const float reference_ms = 120.0f;
    float total = m_ov.lat_valid ? m_ov.lat_total_ms : m_ov.lat_visual_ms;
    float fill = std::clamp(total / reference_ms, 0.0f, 1.0f) * w;

    if (fill > 1.0f)
    {
        float netW = 0.0f;
        if (m_ov.lat_valid && total > 0.0f)
            netW = fill * std::clamp(m_ov.lat_net_ms / total, 0.0f, 1.0f);

        nvgSave(vg);
        nvgScissor(vg, x, barY, fill, barH);

        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, barY, fill, barH, radius);
        nvgFillColor(vg, ovl(p.media, 224));
        nvgFill(vg);

        if (netW > 0.0f)
        {
            nvgBeginPath(vg);
            nvgRoundedRect(vg, x, barY, netW, barH, radius);
            nvgFillColor(vg, ovl(p.accent, 235));
            nvgFill(vg);
        }

        nvgRestore(vg);
    }

    struct Legend { const char* key; const std::string& value; NVGcolor dot; bool dotted; };
    Legend legend[] = {
        { "Net",    m_ov.lat_net,    p.accent, true  },
        { "Visual", m_ov.lat_visual, p.media,  true  },
        { "Decode", m_ov.lat_decode, p.text,   false },
        { "Jitter", m_ov.lat_jitter, p.text,   false },
    };

    float lx = x;
    float ly = y + 36.0f * s;
    for (const Legend& l : legend)
    {
        if (l.dotted)
        {
            nvgBeginPath(vg);
            nvgCircle(vg, lx + 3.0f * s, ly, 3.0f * s);
            nvgFillColor(vg, l.dot);
            nvgFill(vg);
            lx += 10.0f * s;
        }

        nvgFontSize(vg, ovlSize(9.5f * s));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, ovl(p.text, 100));
        nvgText(vg, lx, ly, l.key, nullptr);
        lx += nvgTextBounds(vg, 0, 0, l.key, nullptr, nullptr) + 5.0f * s;

        nvgFontSize(vg, ovlSize(10.5f * s));
        nvgFillColor(vg, ovl(p.text, 200));
        nvgText(vg, lx, ly, l.value.c_str(), nullptr);
        lx += nvgTextBounds(vg, 0, 0, l.value.c_str(), nullptr, nullptr) + 16.0f * s;
    }
}

void VulkanRenderer::drawStatRow(NVGcontext* vg, float x, float y, float w,
                                 const char* label, const std::string& value,
                                 const char* unit, Tone tone)
{
    float s = m_overlay_scale;

    nvgFontSize(vg, ovlSize(11.5f * s));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, ovl(app::views::active().text, 117));
    nvgText(vg, x, y, label, nullptr);

    float right = x + w;
    if (unit && *unit)
    {
        nvgFontSize(vg, ovlSize(10.0f * s));
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, ovl(app::views::active().text, 117));
        nvgText(vg, right, y, unit, nullptr);
        right -= nvgTextBounds(vg, 0, 0, unit, nullptr, nullptr) + 3.0f * s;
    }

    nvgFontSize(vg, ovlSize(12.5f * s));
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, toneColor(tone));
    nvgText(vg, right, y, value.c_str(), nullptr);
}

void VulkanRenderer::drawFsrRow(NVGcontext* vg, float x, float y, float w)
{
    float s = m_overlay_scale;
    const auto& p = app::views::active();

    nvgFontSize(vg, ovlSize(11.5f * s));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, ovl(p.text, 117));
    nvgText(vg, x, y - 4.0f * s, "FSR", nullptr);

    const float dotX = x + 30.0f * s;
    nvgBeginPath(vg);
    nvgCircle(vg, dotX, y - 4.0f * s, 2.5f * s);
    nvgFillColor(vg, p.success);
    nvgFill(vg);

    nvgFontSize(vg, ovlSize(10.5f * s));
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, p.accent);
    nvgText(vg, x + w, y - 4.0f * s, m_ov.fsr_path.c_str(), nullptr);

    nvgFontSize(vg, ovlSize(8.0f * s));
    nvgFillColor(vg, ovl(p.text, 100));
    nvgText(vg, x + w, y + 6.0f * s, m_ov.fsr_detail.c_str(), nullptr);
}

void VulkanRenderer::drawCompactCell(NVGcontext* vg, float& cursorX, float centerY,
                                     const char* label, const std::string& value,
                                     const char* unit, Tone tone, bool first)
{
    float s = m_overlay_scale;
    float pad = 14.0f * s;

    if (!first)
    {
        nvgBeginPath(vg);
        nvgMoveTo(vg, cursorX, centerY - 10.0f * s);
        nvgLineTo(vg, cursorX, centerY + 10.0f * s);
        nvgStrokeWidth(vg, 1.0f);
        nvgStrokeColor(vg, ovl(app::views::active().text, 26));
        nvgStroke(vg);
        cursorX += pad;
    }

    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    nvgFontSize(vg, ovlSize(10.0f * s));
    nvgFillColor(vg, ovl(app::views::active().text, 117));
    nvgText(vg, cursorX, centerY, label, nullptr);
    cursorX += nvgTextBounds(vg, 0, 0, label, nullptr, nullptr) + 7.0f * s;

    nvgFontSize(vg, ovlSize(14.0f * s));
    nvgFillColor(vg, toneColor(tone));
    nvgText(vg, cursorX, centerY, value.c_str(), nullptr);
    cursorX += nvgTextBounds(vg, 0, 0, value.c_str(), nullptr, nullptr);

    if (unit && *unit)
    {
        cursorX += 3.0f * s;
        nvgFontSize(vg, ovlSize(10.5f * s));
        nvgFillColor(vg, ovl(app::views::active().text, 117));
        nvgText(vg, cursorX, centerY, unit, nullptr);
        cursorX += nvgTextBounds(vg, 0, 0, unit, nullptr, nullptr);
    }

    cursorX += pad;
}

void VulkanRenderer::drawCompactFsrLine(NVGcontext* vg, float x, float centerY)
{
    float s = m_overlay_scale;
    const auto& p = app::views::active();

    nvgBeginPath(vg);
    nvgCircle(vg, x + 2.5f * s, centerY, 2.5f * s);
    nvgFillColor(vg, p.success);
    nvgFill(vg);
    x += 9.0f * s;

    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFontSize(vg, ovlSize(10.0f * s));
    nvgFillColor(vg, p.accent);
    nvgText(vg, x, centerY, "FSR", nullptr);
    x += nvgTextBounds(vg, 0, 0, "FSR", nullptr, nullptr) + 7.0f * s;

    nvgFontSize(vg, ovlSize(10.5f * s));
    nvgFillColor(vg, ovl(p.text, 160));
    nvgText(vg, x, centerY, m_ov.fsr_compact.c_str(), nullptr);
}

void VulkanRenderer::overlayOrigin(float w, float h, float& ox, float& oy)
{
    m_overlay_w.store(w, std::memory_order_relaxed);
    m_overlay_h.store(h, std::memory_order_relaxed);
    updateOverlayPlacement();

    ox = m_overlay_to_texture ? 0.0f : m_overlay_x.load(std::memory_order_relaxed);
    oy = m_overlay_to_texture ? 0.0f : m_overlay_y.load(std::memory_order_relaxed);
}

void VulkanRenderer::updateOverlayPlacement()
{
    const float w = m_overlay_w.load(std::memory_order_relaxed);
    const float h = m_overlay_h.load(std::memory_order_relaxed);
    if (w <= 0.0f || h <= 0.0f)
        return;

    const float sw = (float)brls::Application::windowWidth;
    const float sh = (float)brls::Application::windowHeight;
    const float margin = 24.0f * m_overlay_scale;

    const float nx = m_overlay_norm_x.load(std::memory_order_relaxed);
    const float ny = m_overlay_norm_y.load(std::memory_order_relaxed);

    float sx = nx < 0.0f ? margin : nx * sw;
    float sy = ny < 0.0f ? margin : ny * sh;

    sx = std::clamp(sx, margin, std::max(margin, sw - w - margin));
    sy = std::clamp(sy, margin, std::max(margin, sh - h - margin));

    m_overlay_x.store(sx, std::memory_order_relaxed);
    m_overlay_y.store(sy, std::memory_order_relaxed);
}

bool VulkanRenderer::overlayTouchBegin(float x, float y)
{
    std::lock_guard<std::mutex> lock(m_overlay_touch_mutex);
    if (m_stats_mode.load(std::memory_order_relaxed) == StatsOverlayMode::Off)
        return false;

    float ox = m_overlay_x.load(std::memory_order_relaxed);
    float oy = m_overlay_y.load(std::memory_order_relaxed);
    float w = m_overlay_w.load(std::memory_order_relaxed);
    float h = m_overlay_h.load(std::memory_order_relaxed);
    if (w <= 0.0f || h <= 0.0f)
        return false;

    float slop = 12.0f * m_overlay_hit_scale.load(std::memory_order_relaxed);
    if (x < ox - slop || x > ox + w + slop || y < oy - slop || y > oy + h + slop)
    {
        return false;
    }

    m_overlay_grab_dx = x - ox;
    m_overlay_grab_dy = y - oy;
    m_overlay_drag = true;
    return true;
}

void VulkanRenderer::overlayTouchMove(float x, float y)
{
    std::lock_guard<std::mutex> lock(m_overlay_touch_mutex);
    if (!m_overlay_drag)
        return;

    float sw = (float)brls::Application::windowWidth;
    float sh = (float)brls::Application::windowHeight;
    if (sw <= 0.0f || sh <= 0.0f)
        return;

    m_overlay_norm_x.store((x - m_overlay_grab_dx) / sw, std::memory_order_relaxed);
    m_overlay_norm_y.store((y - m_overlay_grab_dy) / sh, std::memory_order_relaxed);
}

void VulkanRenderer::overlayTouchEnd()
{
    std::lock_guard<std::mutex> lock(m_overlay_touch_mutex);
    if (!m_overlay_drag)
        return;
    m_overlay_drag = false;

    float sw = (float)brls::Application::windowWidth;
    float sh = (float)brls::Application::windowHeight;
    if (sw <= 0.0f || sh <= 0.0f)
        return;

    SettingsManager::getInstance()->setStatsOverlayPosition(
        m_overlay_x.load(std::memory_order_relaxed) / sw,
        m_overlay_y.load(std::memory_order_relaxed) / sh);
    m_overlay_pos_dirty.store(true, std::memory_order_relaxed);
}

bool VulkanRenderer::takeOverlayPositionDirty()
{
    return m_overlay_pos_dirty.exchange(false, std::memory_order_relaxed);
}

void VulkanRenderer::drawCompactOverlay(NVGcontext* vg)
{
    float s = m_overlay_scale;
    float margin = 24.0f * s;
    float height = (m_ov.fsr_active ? 58.0f : 36.0f) * s;

    Tone overall = Tone::Good;
    for (Tone t : { toneFps(), toneRate(), toneLoss(), toneRtt(), toneLost(), toneLatency() })
        if (t > overall) overall = t;

    float probe = margin + 14.0f * s;
    float startX = probe;
    drawCompactCell(vg, probe, -10000.0f, "FPS", m_ov.fps, nullptr, Tone::Good, true);
    drawCompactCell(vg, probe, -10000.0f, "RATE", m_ov.rate, "Mbps", Tone::Good, false);
    drawCompactCell(vg, probe, -10000.0f, "LOSS", m_ov.loss, "%", Tone::Good, false);
    drawCompactCell(vg, probe, -10000.0f, "RTT", m_ov.rtt, "ms", Tone::Good, false);
    drawCompactCell(vg, probe, -10000.0f, "LAT", m_ov.lat_total, "ms", Tone::Good, false);
    drawCompactCell(vg, probe, -10000.0f, "UP", m_ov.uptime, nullptr, Tone::Good, false);
    float width = (probe - startX) + 34.0f * s + 13.0f * s;

    float ox, oy;
    overlayOrigin(width, height, ox, oy);

    drawOverlayPanel(vg, ox, oy, width, height);

    float centerY = oy + 18.0f * s;
    float dotX = ox + 14.0f * s + 3.5f * s;
    nvgBeginPath(vg);
    nvgCircle(vg, dotX, centerY, 3.5f * s);
    nvgFillColor(vg, overall == Tone::Good ? app::views::active().success : toneColor(overall));
    nvgFill(vg);

    float cursorX = ox + 14.0f * s + 20.0f * s;
    drawCompactCell(vg, cursorX, centerY, "FPS", m_ov.fps, nullptr, toneFps(), true);
    drawCompactCell(vg, cursorX, centerY, "RATE", m_ov.rate, "Mbps", toneRate(), false);
    drawCompactCell(vg, cursorX, centerY, "LOSS", m_ov.loss, "%", toneLoss(), false);
    drawCompactCell(vg, cursorX, centerY, "RTT", m_ov.rtt, "ms", toneRtt(), false);
    drawCompactCell(vg, cursorX, centerY, "LAT", m_ov.lat_total, "ms", toneLatency(), false);
    drawCompactCell(vg, cursorX, centerY, "UP", m_ov.uptime, nullptr, Tone::Good, false);
    if (m_ov.fsr_active)
    {
        const float dividerY = oy + 36.0f * s;
        nvgBeginPath(vg);
        nvgMoveTo(vg, ox + 14.0f * s, dividerY);
        nvgLineTo(vg, ox + width - 14.0f * s, dividerY);
        nvgStrokeWidth(vg, 1.0f);
        nvgStrokeColor(vg, ovl(app::views::active().text, 26));
        nvgStroke(vg);
        drawCompactFsrLine(vg, ox + 14.0f * s, oy + 47.0f * s);
    }
}

void VulkanRenderer::drawFullOverlay(NVGcontext* vg)
{
    float s = m_overlay_scale;
    float width = 452.0f * s;
    float padX = 16.0f * s;
    float headH = 34.0f * s;
    float rowH = 20.0f * s;
    float footH = 28.0f * s;
    float latH = 48.0f * s;
    const int rowCount = m_ov.fsr_active ? 5 : 4;
    float height = headH + 22.0f * s + rowH * rowCount + 10.0f * s + latH + footH;

    float ox, oy;
    overlayOrigin(width, height, ox, oy);
    float colTop = oy + headH + 22.0f * s;

    drawOverlayPanel(vg, ox, oy, width, height);

    nvgFontSize(vg, ovlSize(12.5f * s));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, app::views::active().accent);
    nvgText(vg, ox + padX, oy + headH * 0.5f, "AKIRA", nullptr);
    float bx = ox + padX + nvgTextBounds(vg, 0, 0, "AKIRA", nullptr, nullptr) + 9.0f * s;
    nvgFillColor(vg, ovl(app::views::active().text, 56));
    nvgText(vg, bx, oy + headH * 0.5f, "/", nullptr);
    nvgFillColor(vg, ovl(app::views::active().text, 184));
    nvgText(vg, bx + 12.0f * s, oy + headH * 0.5f, m_ov.context.c_str(), nullptr);

    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgFontSize(vg, ovlSize(12.0f * s));
    nvgFillColor(vg, ovl(app::views::active().text, 117));
    nvgText(vg, ox + width - padX, oy + headH * 0.5f, m_ov.uptime.c_str(), nullptr);

    nvgBeginPath(vg);
    nvgMoveTo(vg, ox, oy + headH);
    nvgLineTo(vg, ox + width, oy + headH);
    nvgStrokeWidth(vg, 1.0f);
    nvgStrokeColor(vg, ovl(app::views::active().text, 26));
    nvgStroke(vg);

    float colW = (width - padX * 2.0f - 28.0f * s) / 3.0f;
    const char* headers[3] = { "REQUESTED", "OUTPUT", "LINK" };

    for (int c = 0; c < 3; c++)
    {
        float cx = ox + padX + (colW + 14.0f * s) * c;

        if (c > 0)
        {
            nvgBeginPath(vg);
            nvgMoveTo(vg, cx - 7.0f * s, colTop - 14.0f * s);
            nvgLineTo(vg, cx - 7.0f * s, colTop + rowH * rowCount - 4.0f * s);
            nvgStrokeWidth(vg, 1.0f);
            nvgStrokeColor(vg, ovl(app::views::active().text, 26));
            nvgStroke(vg);
        }

        nvgFontSize(vg, ovlSize(9.5f * s));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, ovl(app::views::active().text, 87));
        nvgText(vg, cx, colTop - 14.0f * s, headers[c], nullptr);

        for (int r = 0; r < rowCount; r++)
        {
            float ry = colTop + rowH * r + rowH * 0.5f;
            if (c == 0)
            {
                if (r == 0) drawStatRow(vg, cx, ry, colW, "Res", m_ov.req_res, nullptr, Tone::Good);
                if (r == 1) drawStatRow(vg, cx, ry, colW, "FPS", m_ov.req_fps, nullptr, Tone::Good);
                if (r == 2) drawStatRow(vg, cx, ry, colW, "Rate", m_ov.req_rate, "Mb", Tone::Good);
                if (r == 3) drawStatRow(vg, cx, ry, colW, "Codec", m_ov.req_codec, nullptr, Tone::Good);
            }
            else if (c == 1)
            {
                if (r == 0) drawStatRow(vg, cx, ry, colW, "Res", m_ov.out_res, nullptr, Tone::Good);
                const int offset = m_ov.fsr_active ? 1 : 0;
                if (m_ov.fsr_active && r == 1) drawFsrRow(vg, cx, ry, colW);
                if (r == 1 + offset) drawStatRow(vg, cx, ry, colW, "FPS", m_ov.fps, nullptr, toneFps());
                if (r == 2 + offset) drawStatRow(vg, cx, ry, colW, "Path", m_ov.out_path, nullptr, Tone::Good);
                if (r == 3 + offset) drawStatRow(vg, cx, ry, colW, "Codec", m_ov.out_codec, nullptr, Tone::Good);
            }
            else
            {
                if (r == 0) drawStatRow(vg, cx, ry, colW, "RTT", m_ov.rtt, "ms", toneRtt());
                if (r == 1) drawStatRow(vg, cx, ry, colW, "Rate", m_ov.rate, "Mb", toneRate());
                if (r == 2) drawStatRow(vg, cx, ry, colW, "Loss", m_ov.loss, "%", toneLoss());
                if (r == 3) drawStatRow(vg, cx, ry, colW, "Lost", m_ov.lost, nullptr, toneLost());
            }
        }
    }

    float latY = colTop + rowH * rowCount + 10.0f * s;
    nvgBeginPath(vg);
    nvgMoveTo(vg, ox, latY);
    nvgLineTo(vg, ox + width, latY);
    nvgStrokeWidth(vg, 1.0f);
    nvgStrokeColor(vg, ovl(app::views::active().text, 26));
    nvgStroke(vg);

    drawLatencyStrip(vg, ox + padX, latY + 6.0f * s, width - padX * 2.0f, latH);

    float footY = oy + height - footH;
    nvgBeginPath(vg);
    nvgMoveTo(vg, ox, footY);
    nvgLineTo(vg, ox + width, footY);
    nvgStrokeWidth(vg, 1.0f);
    nvgStrokeColor(vg, ovl(app::views::active().text, 26));
    nvgStroke(vg);

    nvgFontSize(vg, ovlSize(10.5f * s));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    float fx = ox + padX;
    float fy = footY + footH * 0.5f;

    struct { const char* k; const std::string& v; } foot[] = {
        { "SRC", m_ov.src_fps },
        { "GHASH", m_ov.ghash },
        { "VPN", m_ov.vpn },
        { "REC", m_ov.recovered },
    };

    for (const auto& f : foot)
    {
        nvgFillColor(vg, ovl(app::views::active().text, 117));
        nvgText(vg, fx, fy, f.k, nullptr);
        fx += nvgTextBounds(vg, 0, 0, f.k, nullptr, nullptr) + 6.0f * s;
        nvgFillColor(vg, ovl(app::views::active().text, 189));
        nvgText(vg, fx, fy, f.v.c_str(), nullptr);
        fx += nvgTextBounds(vg, 0, 0, f.v.c_str(), nullptr, nullptr) + 18.0f * s;
    }
}

void VulkanRenderer::updateOverlayTexture()
{
    if (m_stats_mode.load(std::memory_order_relaxed) == StatsOverlayMode::Off)
        return;
    if (!m_initialized.load(std::memory_order_acquire))
        return;

    if (!settleOverlay(false))
        return;

    unsigned screenH = brls::Application::windowHeight;
    if (screenH == 0)
        return;

    float scale = (float)screenH / 720.0f;
    if (!(scale > 0.1f && scale < 8.0f))
        scale = 1.0f;

    if (scale != m_overlay_scale)
    {
        m_overlay_scale = scale;
        m_overlay_hit_scale.store(scale, std::memory_order_relaxed);
        m_font_atlas_warm = false;
        m_ovl_dirty = true;
    }

    if (!ensureOverlayTarget())
        return;

    auto now = std::chrono::steady_clock::now();
    if (m_ov_last_rebuild == std::chrono::steady_clock::time_point{} ||
        std::chrono::duration_cast<std::chrono::milliseconds>(now - m_ov_last_rebuild).count() >= 250)
    {
        rebuildOverlayText();
        m_ov_last_rebuild = now;
        m_ovl_dirty = true;
    }

    updateOverlayPlacement();

    if (!m_ovl_dirty)
        return;

    std::shared_ptr<OverlayTarget> target;
    int front = -1;
    {
        std::lock_guard lock(m_ovl_mutex);
        target = m_ovl_target;
        front = m_ovl_front;
    }
    if (!target)
        return;

    const int back = (front == 0) ? 1 : 0;
    if (!renderOverlay(*target, back))
        return;

    m_ovl_in_flight = OverlayInFlight{
        .target = std::move(target),
        .back = back,
        .width = m_overlay_w.load(std::memory_order_relaxed),
        .height = m_overlay_h.load(std::memory_order_relaxed),
    };
    m_ovl_dirty = false;
}

bool VulkanRenderer::settleOverlay(bool wait)
{
    if (!m_ovl_in_flight)
        return true;
    const VkFence fence = m_ovl_context->fence();
    const VkResult status = wait ? vkWaitForFences(m_device, 1, &fence, VK_TRUE, std::numeric_limits<std::uint64_t>::max())
                                 : vkGetFenceStatus(m_device, fence);
    if (status == VK_NOT_READY || status == VK_TIMEOUT)
        return false;
    succeeded(status, "overlay fence");
    succeeded(vkResetFences(m_device, 1, &fence), "vkResetFences (overlay)");
    {
        std::lock_guard lock(m_ovl_mutex);
        if (m_ovl_target == m_ovl_in_flight->target && m_stats_mode.load(std::memory_order_relaxed) != StatsOverlayMode::Off)
        {
            m_ovl_panel_w = m_ovl_in_flight->width;
            m_ovl_panel_h = m_ovl_in_flight->height;
            m_ovl_front = m_ovl_in_flight->back;
        }
    }
    m_ovl_in_flight.reset();
    return true;
}

bool VulkanRenderer::renderOverlay(const OverlayTarget& target, int back)
{
    OverlayContext& context = *m_ovl_context;
    const RenderTarget& color = target.color(back);
    const RenderTarget& stencil = target.stencil();
    const VkExtent2D extent = target.extent();
    const VkFormat stencilFormat = m_vctx->stencilFormat();

    if (!succeeded(vkResetCommandPool(m_device, context.pool(), 0), "vkResetCommandPool (overlay)"))
        return false;

    const VkCommandBufferBeginInfo begin{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    if (!succeeded(vkBeginCommandBuffer(context.upload(), &begin), "vkBeginCommandBuffer (overlay upload)") ||
        !succeeded(vkBeginCommandBuffer(context.draw(), &begin), "vkBeginCommandBuffer (overlay draw)"))
        return false;

    VkCommandBuffer cmd = context.draw();
    imageBarrier(cmd, color.image(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    imageBarrier(cmd, stencil.image(), stencilAspect(stencilFormat), VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

    const VkRenderingAttachmentInfo colorAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = color.view(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = { .color = { { 0.0f, 0.0f, 0.0f, 0.0f } } },
    };
    const VkRenderingAttachmentInfo stencilAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = stencil.view(),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = { .depthStencil = { 1.0f, 0 } },
    };
    const VkRenderingInfo rendering{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { { 0, 0 }, extent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
        .pStencilAttachment = &stencilAttachment,
    };
    vkCmdBeginRendering(cmd, &rendering);
    setViewport(cmd, extent);

    NVGcontext* vg = context.vg();
    context.renderer().setFrame(context.upload(), cmd, 0);
    m_overlay_to_texture = true;
    nvgBeginFrame(vg, static_cast<float>(extent.width), static_cast<float>(extent.height), 1.0f);
    nvgFontFaceId(vg, context.font());
    warmFontAtlas(vg);

    if (m_stats_mode.load(std::memory_order_relaxed) == StatsOverlayMode::Compact)
        drawCompactOverlay(vg);
    else
        drawFullOverlay(vg);

    nvgEndFrame(vg);
    m_overlay_to_texture = false;
    context.renderer().setFrame(VK_NULL_HANDLE, VK_NULL_HANDLE, 0);

    vkCmdEndRendering(cmd);
    imageBarrier(cmd, color.image(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);

    if (!succeeded(vkEndCommandBuffer(context.upload()), "vkEndCommandBuffer (overlay upload)") ||
        !succeeded(vkEndCommandBuffer(cmd), "vkEndCommandBuffer (overlay draw)"))
        return false;

    const std::array commands{
        VkCommandBufferSubmitInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = context.upload() },
        VkCommandBufferSubmitInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = cmd },
    };
    const VkSubmitInfo2 submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = static_cast<std::uint32_t>(commands.size()),
        .pCommandBufferInfos = commands.data(),
    };
    const VkFence fence = context.fence();
    VkResult result = VK_SUCCESS;
    {
        std::lock_guard lock(m_vctx->queueMutex());
        result = vkQueueSubmit2(m_vctx->queue(), 1, &submit, fence);
    }
    return succeeded(result, "vkQueueSubmit2 (overlay)");
}

bool VulkanRenderer::ensureOverlayContext()
{
    if (m_ovl_context)
        return true;

    const VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = m_vctx->queueFamilyIndex(),
    };
    VkCommandPool poolHandle = VK_NULL_HANDLE;
    if (!succeeded(vkCreateCommandPool(m_device, &poolInfo, nullptr, &poolHandle), "vkCreateCommandPool (overlay)"))
        return false;
    CommandPool pool(m_device, poolHandle);

    const VkCommandBufferAllocateInfo allocation{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool.get(),
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 2,
    };
    std::array<VkCommandBuffer, 2> commands{};
    if (!succeeded(vkAllocateCommandBuffers(m_device, &allocation, commands.data()),
            "vkAllocateCommandBuffers (overlay)"))
        return false;

    const VkFenceCreateInfo fenceInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fenceHandle = VK_NULL_HANDLE;
    if (!succeeded(vkCreateFence(m_device, &fenceInfo, nullptr, &fenceHandle), "vkCreateFence (overlay)"))
        return false;
    Fence fence(m_device, fenceHandle);

    auto renderer = std::make_unique<nvg::VkRenderer>(
        nvg::VkRenderer::Device{ m_vctx->physicalDevice(), m_device, m_vctx->memoryProperties(), 1 },
        nvg::VkRenderer::Target{ OffscreenFormat, m_vctx->stencilFormat() });
    NVGcontext* vg = renderer->createContext(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!vg)
    {
        akira::log::error("overlay: private nanovg context creation failed");
        return false;
    }

    const int font = nvgCreateFont(vg, "mono", BRLS_ASSET("font/Cousine-Regular.ttf"));
    auto context = std::make_unique<OverlayContext>(std::move(pool), commands, std::move(fence), std::move(renderer),
        vg, font);
    if (font < 0)
    {
        akira::log::error("overlay: private context could not load the mono font");
        return false;
    }

    m_ovl_context = std::move(context);
    m_font_atlas_warm = false;
    akira::log::info("overlay: private nanovg context ready");
    return true;
}

void VulkanRenderer::destroyOverlayContext()
{
    if (m_ovl_context)
        settleOverlay(true);
    m_ovl_context.reset();
    m_font_atlas_warm = false;
}

bool VulkanRenderer::ensureOverlayTarget()
{
    const float s = m_overlay_scale;
    const VkExtent2D want{ static_cast<std::uint32_t>(620.0f * s) + 4, static_cast<std::uint32_t>(240.0f * s) + 4 };

    {
        std::lock_guard lock(m_ovl_mutex);
        if (m_ovl_target && m_ovl_target->extent().width == want.width &&
            m_ovl_target->extent().height == want.height)
            return true;
    }

    if (!ensureOverlayContext())
        return false;

    auto pool = createDescriptorPool(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, 2);
    if (!pool)
        return false;

    constexpr VkImageUsageFlags colorUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    auto first = createRenderTarget(want, OffscreenFormat, colorUsage, VK_IMAGE_ASPECT_COLOR_BIT, pool->get());
    auto second = createRenderTarget(want, OffscreenFormat, colorUsage, VK_IMAGE_ASPECT_COLOR_BIT, pool->get());
    auto stencil = createRenderTarget(want, m_vctx->stencilFormat(), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        stencilAspect(m_vctx->stencilFormat()), VK_NULL_HANDLE);
    if (!first || !second || !stencil)
    {
        akira::log::error("overlay: could not allocate {}x{} render targets", want.width, want.height);
        return false;
    }

    auto target = std::make_shared<OverlayTarget>(std::move(*pool),
        std::array<RenderTarget, 2>{ std::move(*first), std::move(*second) }, std::move(*stencil));
    {
        std::lock_guard lock(m_ovl_mutex);
        std::swap(m_ovl_target, target);
        m_ovl_front = -1;
    }
    m_ovl_dirty = true;

    akira::log::info("overlay: render targets {}x{} ready", want.width, want.height);
    return true;
}

bool VulkanRenderer::captureLastFrame(std::vector<uint8_t>& rgba, int& width, int& height)
{
    if (!m_initialized.load(std::memory_order_acquire))
        return false;

    Frame held;
    {
        std::lock_guard lock(m_frame_mutex);
        held = m_last_frame;
    }
    if (!held || held->width() == 0 || held->height() == 0 || !held->backing())
        return false;

    const PlaneLayout layout = planeLayout(*held);
    const int srcW = static_cast<int>(layout.width);
    const int srcH = static_cast<int>(layout.height);

    int dstW = CAPTURE_WIDTH;
    int dstH = (int)((int64_t)dstW * srcH / srcW) & ~1;
    if (dstH <= 0)
        dstH = CAPTURE_WIDTH * 9 / 16;

    using Scaler = std::unique_ptr<SwsContext, decltype(&sws_freeContext)>;
    Scaler sws(sws_getContext(srcW, srcH, AV_PIX_FMT_NV12, dstW, dstH, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr,
                   nullptr, nullptr),
        &sws_freeContext);
    if (!sws)
        return false;
    sws_setColorspaceDetails(sws.get(), sws_getCoefficients(SWS_CS_ITU709), 0, sws_getCoefficients(SWS_CS_DEFAULT),
        1, 0, 1 << 16, 1 << 16);

    const auto* base = static_cast<const uint8_t*>(layout.backing);
    const std::array<const uint8_t*, 4> src{ base + layout.luma.offset, base + layout.chroma.offset, nullptr, nullptr };
    const std::array<int, 4> srcStride{ static_cast<int>(layout.luma.pitch), static_cast<int>(layout.chroma.pitch),
        0, 0 };

    rgba.assign((size_t)dstW * dstH * 4, 0);
    const std::array<uint8_t*, 4> dst{ rgba.data(), nullptr, nullptr, nullptr };
    const std::array<int, 4> dstStride{ dstW * 4, 0, 0, 0 };

    const int scaled = sws_scale(sws.get(), src.data(), srcStride.data(), 0, srcH, dst.data(), dstStride.data());
    if (scaled <= 0)
    {
        rgba.clear();
        return false;
    }

    width = dstW;
    height = dstH;
    return true;
}

}

