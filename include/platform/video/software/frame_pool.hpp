#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <span>

#include "platform/video/nv12_frame.hpp"

namespace akira::platform::software
{

class FramePool;

class PooledFrame final : public akira::platform::Nv12Frame
{
public:
    PooledFrame(std::shared_ptr<FramePool> pool, std::uint32_t slot, std::int64_t timestampNs);
    ~PooledFrame() override;
    PooledFrame(const PooledFrame&) = delete;
    PooledFrame& operator=(const PooledFrame&) = delete;

    std::uint32_t width() const override;
    std::uint32_t height() const override;
    std::uint32_t slot() const override { return m_slot; }
    std::int64_t timestampNs() const override { return m_timestamp_ns; }
    std::uint64_t generation() const override;
    const void* backing() const override;
    std::size_t backingSize() const override;
    akira::platform::Nv12Plane luma() const override;
    akira::platform::Nv12Plane chroma() const override;

    std::span<std::byte> bytes();

private:
    std::shared_ptr<FramePool> m_pool;
    std::uint32_t m_slot;
    std::int64_t m_timestamp_ns;
};

class FramePool : public std::enable_shared_from_this<FramePool>
{
public:
    static constexpr std::uint32_t Slots = 6;

    static std::optional<std::shared_ptr<FramePool>> create(std::uint32_t width, std::uint32_t height);

    FramePool(const FramePool&) = delete;
    FramePool& operator=(const FramePool&) = delete;

    std::optional<std::shared_ptr<PooledFrame>> acquire(std::int64_t timestampNs);

    std::uint32_t width() const { return m_width; }
    std::uint32_t height() const { return m_height; }
    std::uint64_t generation() const { return m_generation; }
    std::size_t backingSize() const { return m_backing_size; }
    akira::platform::Nv12Plane luma() const { return {0, m_pitch}; }
    akira::platform::Nv12Plane chroma() const { return {m_chroma_offset, m_pitch}; }

private:
    friend class PooledFrame;

    static constexpr std::size_t PitchAlignment = 256;
    static constexpr std::size_t BackingAlignment = 0x10000;

    struct AlignedFree
    {
        void operator()(std::byte* bytes) const { std::free(bytes); }
    };

    FramePool(std::uint32_t width, std::uint32_t height);
    std::byte* slotBytes(std::uint32_t slot) const { return m_slots[slot].get(); }
    void release(std::uint32_t slot);

    std::uint32_t m_width;
    std::uint32_t m_height;
    std::uint32_t m_pitch;
    std::size_t m_chroma_offset;
    std::size_t m_backing_size;
    std::uint64_t m_generation;
    std::array<std::unique_ptr<std::byte, AlignedFree>, Slots> m_slots;
    std::array<std::atomic<bool>, Slots> m_busy{};
};

}
