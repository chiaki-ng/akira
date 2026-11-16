#include "platform/video/viddec/output_image.hpp"

#include <algorithm>
#include <cstddef>

namespace akira::platform::viddec
{

std::expected<std::unique_ptr<OutputImage>, OutputFailure> OutputImage::acquire(OutputMemory& memory,
    const GsvDecodeImagePreference& preference)
{
    std::unique_ptr<OutputImage> output(new OutputImage(preference));
    if (auto built = output->build(memory); !built)
        return std::unexpected(built.error());
    return output;
}

OutputImage::OutputImage(const GsvDecodeImagePreference& preference)
    : m_preference(preference)
{
}

OutputImage::~OutputImage()
{
    if (m_image.pool.base_image_id)
        gsvDestroyDecodeImagePool(m_image.pool.base_image_id);
    if (m_memory_id)
        gsvReleaseImageMemory(m_memory_id);
}

std::expected<void, OutputFailure> OutputImage::build(OutputMemory& memory)
{
    u64 size = 0;
    u64 alignment = 0;
    if (const Result rc = viddecGetImagePoolSize(&m_preference, 1, &size, &alignment); R_FAILED(rc) || !size)
        return std::unexpected(OutputFailure{"viddecGetImagePoolSize", rc});

    auto block = memory.allocate(size, alignment);
    if (!block)
        return std::unexpected(block.error());
    m_block = std::move(block).value();
    const std::span<std::byte> bytes = m_block->bytes();
    std::ranges::fill(bytes, std::byte{0});
    armDCacheFlush(bytes.data(), bytes.size());

    if (const Result rc = gsvReserveImageMemory(NativeMemoryType, bytes.size(), &m_memory_id); R_FAILED(rc) || !m_memory_id)
        return std::unexpected(OutputFailure{"gsvReserveImageMemory", rc});
    if (const Result rc = registerNative(); R_FAILED(rc))
        return std::unexpected(OutputFailure{"register native memory", rc});

    u32 base = 0;
    if (const Result rc = gsvCreateDecodeImagePool(&m_preference, 1, m_memory_id, &base); R_FAILED(rc) || !base)
        return std::unexpected(OutputFailure{"gsvCreateDecodeImagePool", rc});
    m_image.pool.base_image_id = base;
    m_image.pool.count = 1;
    m_image.pool.backing = bytes.data();
    m_image.pool.backing_size = bytes.size();
    m_image.backing = bytes.data();
    m_image.backing_size = bytes.size();
    if (const Result rc = gsvGetCpuImageDescriptor(base, &m_image.descriptor); R_FAILED(rc))
        return std::unexpected(OutputFailure{"gsvGetCpuImageDescriptor", rc});
    if (m_image.descriptor.type != GsvCpuImageType_Nv12PitchLinear && m_image.descriptor.type != GsvCpuImageType_Nv12PitchLinearAlt)
        return std::unexpected(OutputFailure{"pitch-linear NV12 descriptor", m_image.descriptor.type});
    return {};
}

Result OutputImage::registerNative()
{
    struct __attribute__((packed)) Request
    {
        u32 memory_id;
        u64 address;
        u64 size;
        u64 flags;
        u64 reserved;
    };
    const std::span<std::byte> bytes = m_block->bytes();
    const Request request{m_memory_id, reinterpret_cast<std::uintptr_t>(bytes.data()), bytes.size(), 0, 0};
    return tipcDispatchIn(gsvGetServiceSession(), RegisterNativeMemory, request);
}

}
