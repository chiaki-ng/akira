#pragma once

#include <switch.h>

#include <cstdint>
#include <expected>
#include <memory>

#include "platform/video/viddec/output_memory.hpp"

namespace akira::platform::viddec
{

class OutputImage
{
public:
    static std::expected<std::unique_ptr<OutputImage>, OutputFailure> acquire(OutputMemory& memory,
        const GsvDecodeImagePreference& preference);

    ~OutputImage();
    OutputImage(const OutputImage&) = delete;
    OutputImage& operator=(const OutputImage&) = delete;

    const VideoDecoderImage& image() const { return m_image; }
    OutputHandle handle() const { return m_block->handle(); }

private:
    static constexpr std::uint8_t NativeMemoryType = 3;
    static constexpr std::uint32_t RegisterNativeMemory = 114;

    explicit OutputImage(const GsvDecodeImagePreference& preference);
    std::expected<void, OutputFailure> build(OutputMemory& memory);
    Result registerNative();

    GsvDecodeImagePreference m_preference;
    std::unique_ptr<OutputBlock> m_block;
    std::uint32_t m_memory_id = 0;
    VideoDecoderImage m_image{};
};

}
