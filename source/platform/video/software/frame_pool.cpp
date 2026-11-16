#include "platform/video/software/frame_pool.hpp"

#include <utility>

namespace akira::platform::software
{

PooledFrame::PooledFrame(std::shared_ptr<FramePool> pool, std::uint32_t slot, std::int64_t timestampNs)
    : m_pool(std::move(pool)), m_slot(slot), m_timestamp_ns(timestampNs)
{
}

PooledFrame::~PooledFrame()
{
    m_pool->release(m_slot);
}

std::uint32_t PooledFrame::width() const
{
    return m_pool->width();
}

std::uint32_t PooledFrame::height() const
{
    return m_pool->height();
}

std::uint64_t PooledFrame::generation() const
{
    return m_pool->generation();
}

const void* PooledFrame::backing() const
{
    return m_pool->slotBytes(m_slot);
}

std::size_t PooledFrame::backingSize() const
{
    return m_pool->backingSize();
}

akira::platform::Nv12Plane PooledFrame::luma() const
{
    return m_pool->luma();
}

akira::platform::Nv12Plane PooledFrame::chroma() const
{
    return m_pool->chroma();
}

std::span<std::byte> PooledFrame::bytes()
{
    return {m_pool->slotBytes(m_slot), m_pool->backingSize()};
}

FramePool::FramePool(std::uint32_t width, std::uint32_t height)
    : m_width(width)
    , m_height(height)
    , m_pitch(static_cast<std::uint32_t>((width + PitchAlignment - 1) / PitchAlignment * PitchAlignment))
    , m_chroma_offset(std::size_t{m_pitch} * height)
    , m_backing_size((m_chroma_offset + std::size_t{m_pitch} * ((height + 1) / 2) + BackingAlignment - 1)
          / BackingAlignment * BackingAlignment)
    , m_generation(akira::platform::Nv12Frame::nextGeneration())
{
}

std::optional<std::shared_ptr<FramePool>> FramePool::create(std::uint32_t width, std::uint32_t height)
{
    std::shared_ptr<FramePool> pool(new FramePool(width, height));
    for (auto& slot : pool->m_slots)
    {
        slot.reset(static_cast<std::byte*>(std::aligned_alloc(BackingAlignment, pool->m_backing_size)));
        if (!slot)
            return std::nullopt;
    }
    return pool;
}

std::optional<std::shared_ptr<PooledFrame>> FramePool::acquire(std::int64_t timestampNs)
{
    for (std::uint32_t slot = 0; slot < Slots; ++slot)
    {
        bool expected = false;
        if (m_busy[slot].compare_exchange_strong(expected, true, std::memory_order_acquire))
            return std::make_shared<PooledFrame>(shared_from_this(), slot, timestampNs);
    }
    return std::nullopt;
}

void FramePool::release(std::uint32_t slot)
{
    m_busy[slot].store(false, std::memory_order_release);
}

}
