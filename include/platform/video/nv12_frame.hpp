#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "platform/video/viddec/output_memory.hpp"

namespace akira::platform
{

struct Nv12Plane
{
    std::size_t offset;
    std::uint32_t pitch;
};

class Nv12Frame
{
public:
    static constexpr std::uint32_t SlotCount = 8;

    virtual ~Nv12Frame() = default;

    virtual std::uint32_t width() const = 0;
    virtual std::uint32_t height() const = 0;
    virtual std::uint32_t slot() const = 0;
    virtual std::int64_t timestampNs() const = 0;
    virtual std::uint64_t generation() const = 0;
    virtual const void* backing() const = 0;
    virtual std::size_t backingSize() const = 0;
    virtual Nv12Plane luma() const = 0;
    virtual Nv12Plane chroma() const = 0;
    virtual std::optional<viddec::OutputHandle> output() const { return std::nullopt; }

    static std::uint64_t nextGeneration() { return ++s_generations; }

protected:
    Nv12Frame() = default;
    Nv12Frame(const Nv12Frame&) = default;
    Nv12Frame& operator=(const Nv12Frame&) = default;

private:
    static inline std::atomic<std::uint64_t> s_generations{0};
};

}
