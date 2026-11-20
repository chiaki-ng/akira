#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "play/switch_button.hpp"

namespace akira::play {

class ComboCapture {
public:
    static constexpr int graceFrames = 6;

    static auto buttons() -> const std::vector<SwitchButton>&;

    auto feed(std::uint64_t held) -> std::optional<std::vector<std::uint64_t>>;
    auto peak() const -> std::vector<std::uint64_t>;
    auto holding(SwitchButton button) const -> bool { return (peakBits & bits(button)) != 0; }

private:
    std::uint64_t peakBits = 0;
    bool waitingForRelease = true;
    bool pressed = false;
    int quietFrames = 0;
};

}
