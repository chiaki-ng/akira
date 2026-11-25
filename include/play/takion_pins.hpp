#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace akira::play {

class TakionPins {
public:
    static constexpr std::array<std::uint32_t, 2> ps4{9, 12};
    static constexpr std::array<std::uint32_t, 4> ps5{12, 15, 19, 20};

    static constexpr std::span<const std::uint32_t> pinnable(bool isPs5)
    {
        if (isPs5)
            return ps5;
        return ps4;
    }

    static constexpr bool isPinnable(bool isPs5, std::uint32_t version)
    {
        return std::ranges::contains(pinnable(isPs5), version);
    }
};

}
