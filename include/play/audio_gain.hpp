#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

#include "play/volume.hpp"

namespace akira::play {

class AudioGain {
public:
    static constexpr double boost = 1.80;

    void setVolume(Volume volume);
    auto apply(std::span<const std::int16_t> samples, std::span<std::int16_t> into) const -> std::size_t;

private:
    std::atomic<double> factor{ boost };
};

}
