#include "stream/decoder_output.hpp"

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <cstring>

namespace akira::stream
{

std::expected<std::unique_ptr<DecoderOutput>, DecoderOutput::Failure> DecoderOutput::acquire(const Device& device,
    const GsvDecodeImagePreference& preference)
{
    std::unique_ptr<DecoderOutput> output(new DecoderOutput(device, preference));
    if (auto built = output->build(); !built)
        return std::unexpected(built.error());
    return output;
}

DecoderOutput::DecoderOutput(const Device& device, const GsvDecodeImagePreference& preference)
    : m_device(device), m_preference(preference)
{
}

DecoderOutput::~DecoderOutput()
{
    if (m_image.pool.base_image_id)
        gsvDestroyDecodeImagePool(m_image.pool.base_image_id);
    if (m_memory_id)
        gsvReleaseImageMemory(m_memory_id);
    if (m_buffer)
        vkDestroyBuffer(m_device.device, m_buffer, nullptr);
    if (m_memory)
        vkFreeMemory(m_device.device, m_memory, nullptr);
    else
        std::free(m_heap);
}

std::expected<void, DecoderOutput::Failure> DecoderOutput::build()
{
    u64 size = 0;
    u64 alignment = 0;
    if (const Result rc = viddecGetImagePoolSize(&m_preference, 1, &size, &alignment); R_FAILED(rc) || !size)
        return std::unexpected(Failure{"viddecGetImagePoolSize", rc});

    VkPhysicalDeviceExternalMemoryHostPropertiesEXT host{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT,
    };
    VkPhysicalDeviceProperties2 properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &host,
    };
    vkGetPhysicalDeviceProperties2(m_device.physical, &properties);
    const std::uint64_t align = std::max<std::uint64_t>({alignment, host.minImportedHostPointerAlignment, PageSize});
    m_size = (size + align - 1) & ~(align - 1);
    m_heap = std::aligned_alloc(align, m_size);
    if (!m_heap)
        return std::unexpected(Failure{"aligned_alloc", static_cast<std::int64_t>(m_size)});
    std::memset(m_heap, 0, m_size);
    armDCacheFlush(m_heap, m_size);

    if (auto imported = import(); !imported)
        return imported;

    if (const Result rc = gsvReserveImageMemory(NativeMemoryType, m_size, &m_memory_id); R_FAILED(rc) || !m_memory_id)
        return std::unexpected(Failure{"gsvReserveImageMemory", rc});
    if (const Result rc = registerNative(); R_FAILED(rc))
        return std::unexpected(Failure{"register native memory", rc});

    u32 base = 0;
    if (const Result rc = gsvCreateDecodeImagePool(&m_preference, 1, m_memory_id, &base); R_FAILED(rc) || !base)
        return std::unexpected(Failure{"gsvCreateDecodeImagePool", rc});
    m_image.pool.base_image_id = base;
    m_image.pool.count = 1;
    m_image.pool.backing = m_heap;
    m_image.pool.backing_size = m_size;
    m_image.backing = m_heap;
    m_image.backing_size = m_size;
    if (const Result rc = gsvGetCpuImageDescriptor(base, &m_image.descriptor); R_FAILED(rc))
        return std::unexpected(Failure{"gsvGetCpuImageDescriptor", rc});
    if (m_image.descriptor.type != GsvCpuImageType_Nv12PitchLinear && m_image.descriptor.type != GsvCpuImageType_Nv12PitchLinearAlt)
        return std::unexpected(Failure{"pitch-linear NV12 descriptor", m_image.descriptor.type});
    return {};
}

std::expected<void, DecoderOutput::Failure> DecoderOutput::import()
{
    const auto properties = reinterpret_cast<PFN_vkGetMemoryHostPointerPropertiesEXT>(
        vkGetDeviceProcAddr(m_device.device, "vkGetMemoryHostPointerPropertiesEXT"));
    if (!properties)
        return std::unexpected(Failure{"vkGetMemoryHostPointerPropertiesEXT", 0});
    VkMemoryHostPointerPropertiesEXT pointer{.sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT};
    if (const VkResult r = properties(m_device.device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, m_heap, &pointer);
        r != VK_SUCCESS)
        return std::unexpected(Failure{"vkGetMemoryHostPointerPropertiesEXT", r});

    const VkExternalMemoryBufferCreateInfo external{
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
    };
    const VkBufferCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = &external,
        .size = m_size,
        .usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    if (const VkResult r = vkCreateBuffer(m_device.device, &info, nullptr, &m_buffer); r != VK_SUCCESS)
        return std::unexpected(Failure{"vkCreateBuffer", r});
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(m_device.device, m_buffer, &requirements);
    const std::uint32_t types = requirements.memoryTypeBits & pointer.memoryTypeBits;
    if (!types || requirements.size > m_size)
        return std::unexpected(Failure{"host import memory type", types});

    const VkImportMemoryHostPointerInfoEXT hostPointer{
        .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
        .pHostPointer = m_heap,
    };
    const VkMemoryAllocateInfo allocate{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &hostPointer,
        .allocationSize = m_size,
        .memoryTypeIndex = static_cast<std::uint32_t>(std::countr_zero(types)),
    };
    if (const VkResult r = vkAllocateMemory(m_device.device, &allocate, nullptr, &m_memory); r != VK_SUCCESS)
        return std::unexpected(Failure{"vkAllocateMemory (host import)", r});
    if (const VkResult r = vkBindBufferMemory(m_device.device, m_buffer, m_memory, 0); r != VK_SUCCESS)
        return std::unexpected(Failure{"vkBindBufferMemory", r});
    return {};
}

Result DecoderOutput::registerNative()
{
    struct __attribute__((packed)) Request
    {
        u32 memory_id;
        u64 address;
        u64 size;
        u64 flags;
        u64 reserved;
    };
    const Request request{m_memory_id, reinterpret_cast<std::uintptr_t>(m_heap), m_size, 0, 0};
    return tipcDispatchIn(gsvGetServiceSession(), RegisterNativeMemory, request);
}

}
