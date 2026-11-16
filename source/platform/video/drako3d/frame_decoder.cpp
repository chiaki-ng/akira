#include "platform/video/drako3d/frame_decoder.hpp"

#include <memory>
#include <utility>

#include <switch.h>

#include "platform/video/drako3d/output_memory.hpp"

namespace akira::platform::drako3d
{

ViddecFrameDecoder::ViddecFrameDecoder()
{
    m_decoder.setOutputMemory(std::make_shared<HostOutputMemory>());
}

void ViddecFrameDecoder::setFrameReadyCallback(FrameReadyCallback callback)
{
    if (!callback)
    {
        m_decoder.setFrameReadyCallback(nullptr);
        return;
    }
    m_decoder.setFrameReadyCallback([callback = std::move(callback)](viddec::ViddecDecoder::Frame frame) {
        if (frame)
            callback(describe(frame));
    });
}

deko3d::VideoFrame ViddecFrameDecoder::describe(const viddec::ViddecDecoder::Frame& frame)
{
    constexpr std::size_t page = HostOutputMemory::PageSize;
    auto* storage = const_cast<void*>(frame->backing());
    const std::size_t size = (frame->backingSize() + page - 1) & ~(page - 1);
    return deko3d::VideoFrame{
        .width = static_cast<int>(frame->width()),
        .height = static_cast<int>(frame->height()),
        .storage = storage,
        .storageSize = size,
        .chromaOffset = frame->chroma().offset,
        .lumaPitch = frame->luma().pitch,
        .chromaPitch = frame->chroma().pitch,
        .layout = deko3d::PlaneLayout::PitchLinear,
        .cacheKey = (frame->generation() << 8) | frame->slot(),
        .lifetime = frame,
        .prepareCpuRead = [storage, size] { armDCacheFlush(storage, size); },
    };
}

}
