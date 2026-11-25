#pragma once

#include <cstdint>

#include "play/switch_button.hpp"

namespace akira::play {

class ScreenshotChord {
public:
    static constexpr std::uint64_t triggers = bits(SwitchButton::ZL) | bits(SwitchButton::ZR);
    static constexpr std::uint64_t stick = bits(SwitchButton::StickR);
    static constexpr std::uint64_t chord = triggers | stick;

    auto update(std::uint64_t held) -> bool;
    auto swallowing() const -> bool { return swallowed; }
    auto filtered(std::uint64_t held) const -> std::uint64_t { return swallowed ? held & ~stick : held; }

private:
    bool complete = false;
    bool swallowed = false;
};

}
