#include "platform/video/vulkan/decoder_output.hpp"

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <span>
#include <utility>

namespace akira::platform::vulkan
{

using akira::platform::viddec::OutputBlock;
using akira::platform::viddec::OutputFailure;
using akira::platform::viddec::OutputHandle;

class DecoderOutput::Block final : public OutputBlock
{
public:
    Block(VkDevice device, std::uint64_t size)
        : m_device(device), m_size(size)
    {
    }

    ~Block() override
    {
        if (m_buffer)
            vkDestroyBuffer(m_device, m_buffer, nullptr);
        if (m_memory)
            vkFreeMemory(m_device, m_memory, nullptr);
        else
            std::free(m_heap);
    }

    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;

    std::span<std::byte> bytes() const override { return {static_cast<std::byte*>(m_heap), m_size}; }
    OutputHandle handle() const override { return OutputHandle{std::bit_cast<std::uint64_t>(m_buffer)}; }

    std::expected<void, OutputFailure> build(std::uint64_t alignment)
    {
        m_heap = std::aligned_alloc(alignment, m_size);
        if (!m_heap)
            return std::unexpected(OutputFailure{"aligned_alloc", static_cast<std::int64_t>(m_size)});
        return import();
    }

private:
    std::expected<void, OutputFailure> import()
    {
        const auto properties = reinterpret_cast<PFN_vkGetMemoryHostPointerPropertiesEXT>(
            vkGetDeviceProcAddr(m_device, "vkGetMemoryHostPointerPropertiesEXT"));
        if (!properties)
            return std::unexpected(OutputFailure{"vkGetMemoryHostPointerPropertiesEXT", 0});
        VkMemoryHostPointerPropertiesEXT pointer{.sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT};
        if (const VkResult r = properties(m_device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, m_heap, &pointer);
            r != VK_SUCCESS)
            return std::unexpected(OutputFailure{"vkGetMemoryHostPointerPropertiesEXT", r});

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
        if (const VkResult r = vkCreateBuffer(m_device, &info, nullptr, &m_buffer); r != VK_SUCCESS)
            return std::unexpected(OutputFailure{"vkCreateBuffer", r});
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(m_device, m_buffer, &requirements);
        const std::uint32_t types = requirements.memoryTypeBits & pointer.memoryTypeBits;
        if (!types || requirements.size > m_size)
            return std::unexpected(OutputFailure{"host import memory type", types});

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
        if (const VkResult r = vkAllocateMemory(m_device, &allocate, nullptr, &m_memory); r != VK_SUCCESS)
            return std::unexpected(OutputFailure{"vkAllocateMemory (host import)", r});
        if (const VkResult r = vkBindBufferMemory(m_device, m_buffer, m_memory, 0); r != VK_SUCCESS)
            return std::unexpected(OutputFailure{"vkBindBufferMemory", r});
        return {};
    }

    VkDevice m_device;
    std::uint64_t m_size;
    void* m_heap = nullptr;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkBuffer m_buffer = VK_NULL_HANDLE;
};

DecoderOutput::DecoderOutput(VkDevice device, VkPhysicalDevice physical)
    : m_device(device), m_physical(physical)
{
}

std::expected<std::unique_ptr<OutputBlock>, OutputFailure> DecoderOutput::allocate(std::uint64_t size, std::uint64_t alignment)
{
    VkPhysicalDeviceExternalMemoryHostPropertiesEXT host{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT,
    };
    VkPhysicalDeviceProperties2 properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &host,
    };
    vkGetPhysicalDeviceProperties2(m_physical, &properties);
    const std::uint64_t align = std::max<std::uint64_t>({alignment, host.minImportedHostPointerAlignment, PageSize});
    auto block = std::make_unique<Block>(m_device, (size + align - 1) & ~(align - 1));
    if (auto built = block->build(align); !built)
        return std::unexpected(built.error());
    return block;
}

VkBuffer DecoderOutput::buffer(OutputHandle handle)
{
    return std::bit_cast<VkBuffer>(std::to_underlying(handle));
}

}
