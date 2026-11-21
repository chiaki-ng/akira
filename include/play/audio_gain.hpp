#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace akira::play {

class AudioGain {
public:
    static constexpr double boost = 1.80;

    static auto apply(std::span<const std::int16_t> samples, std::span<std::int16_t> into) -> std::size_t;
};

}
