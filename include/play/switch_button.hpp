#pragma once

#include <cstdint>

namespace akira::play {

enum class SwitchButton : uint64_t {
    A = 1ull << 0,
    B = 1ull << 1,
    X = 1ull << 2,
    Y = 1ull << 3,
    StickL = 1ull << 4,
    StickR = 1ull << 5,
    L = 1ull << 6,
    R = 1ull << 7,
    ZL = 1ull << 8,
    ZR = 1ull << 9,
    Plus = 1ull << 10,
    Minus = 1ull << 11,
    Left = 1ull << 12,
    Up = 1ull << 13,
    Right = 1ull << 14,
    Down = 1ull << 15,
    StickLLeft = 1ull << 16,
    StickLUp = 1ull << 17,
    StickLRight = 1ull << 18,
    StickLDown = 1ull << 19,
    StickRLeft = 1ull << 20,
    StickRUp = 1ull << 21,
    StickRRight = 1ull << 22,
    StickRDown = 1ull << 23,
    LeftSL = 1ull << 24,
    LeftSR = 1ull << 25,
    RightSL = 1ull << 26,
    RightSR = 1ull << 27,
};

constexpr uint64_t bits(SwitchButton button)
{
    return static_cast<uint64_t>(button);
}

}
