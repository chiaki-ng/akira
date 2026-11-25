#include "play/audio_gain.hpp"

#include <algorithm>
#include <limits>

namespace akira::play {

void AudioGain::setVolume(Volume volume)
{
    factor.store(boost * volume.level(), std::memory_order_relaxed);
}

auto AudioGain::apply(std::span<const std::int16_t> samples, std::span<std::int16_t> into) const -> std::size_t
{
    const double scale = factor.load(std::memory_order_relaxed);
    const std::size_t count = std::min(samples.size(), into.size());
    std::ranges::transform(samples.first(count), into.begin(), [scale](std::int16_t sample) {
        const auto boosted = static_cast<int>(sample * scale);
        return static_cast<std::int16_t>(std::clamp(boosted, static_cast<int>(std::numeric_limits<std::int16_t>::min()),
            static_cast<int>(std::numeric_limits<std::int16_t>::max())));
    });
    return count;
}

}
