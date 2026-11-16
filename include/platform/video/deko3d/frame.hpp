#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

namespace akira::platform::deko3d
{

enum class PlaneLayout
{
    PitchLinear,
    BlockLinear,
};

struct VideoFrame
{
    int width = 0;
    int height = 0;
    void* storage = nullptr;
    std::size_t storageSize = 0;
    std::size_t chromaOffset = 0;
    std::uint32_t lumaPitch = 0;
    std::uint32_t chromaPitch = 0;
    PlaneLayout layout = PlaneLayout::PitchLinear;
    std::uint32_t blockHeightLog2 = 0;
    std::uint64_t cacheKey = 0;
    std::shared_ptr<const void> lifetime;
    std::function<void()> prepareCpuRead;
};

}
