#include "play/audio_gain.hpp"

#include <algorithm>
#include <limits>

namespace akira::play {

auto AudioGain::apply(std::span<const std::int16_t> samples, std::span<std::int16_t> into) -> std::size_t
{
    const std::size_t count = std::min(samples.size(), into.size());
    std::ranges::transform(samples.first(count), into.begin(), [](std::int16_t sample) {
        const int boosted = static_cast<int>(sample * boost);
        return static_cast<std::int16_t>(std::clamp(boosted, static_cast<int>(std::numeric_limits<std::int16_t>::min()),
            static_cast<int>(std::numeric_limits<std::int16_t>::max())));
    });
    return count;
}

}
