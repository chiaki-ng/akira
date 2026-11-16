#ifndef AKIRA_STREAM_DECODER_OUTPUT_HPP
#define AKIRA_STREAM_DECODER_OUTPUT_HPP

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>

#include "platform/video/viddec/output_memory.hpp"

namespace akira::platform::vulkan
{

class DecoderOutput final : public akira::platform::viddec::OutputMemory
{
public:
    DecoderOutput(VkDevice device, VkPhysicalDevice physical);

    std::expected<std::unique_ptr<akira::platform::viddec::OutputBlock>, akira::platform::viddec::OutputFailure> allocate(
        std::uint64_t size, std::uint64_t alignment) override;

    static VkBuffer buffer(akira::platform::viddec::OutputHandle handle);

private:
    static constexpr std::uint64_t PageSize = 0x1000;

    class Block;

    VkDevice m_device;
    VkPhysicalDevice m_physical;
};

}

#endif
