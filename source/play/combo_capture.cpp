#include "play/combo_capture.hpp"

namespace akira::play {

auto ComboCapture::buttons() -> const std::vector<SwitchButton>&
{
    static const std::vector<SwitchButton> all{
        SwitchButton::A, SwitchButton::B, SwitchButton::X, SwitchButton::Y,
        SwitchButton::L, SwitchButton::R, SwitchButton::ZL, SwitchButton::ZR,
        SwitchButton::Plus, SwitchButton::Minus, SwitchButton::StickL, SwitchButton::StickR,
        SwitchButton::LeftSL, SwitchButton::LeftSR, SwitchButton::RightSL, SwitchButton::RightSR,
        SwitchButton::Up, SwitchButton::Down, SwitchButton::Left, SwitchButton::Right,
        SwitchButton::StickLUp, SwitchButton::StickLDown, SwitchButton::StickLLeft, SwitchButton::StickLRight,
        SwitchButton::StickRUp, SwitchButton::StickRDown, SwitchButton::StickRLeft, SwitchButton::StickRRight,
    };
    return all;
}

auto ComboCapture::feed(std::uint64_t held) -> std::optional<std::vector<std::uint64_t>>
{
    if (waitingForRelease)
    {
        if (held == 0)
            waitingForRelease = false;
        return std::nullopt;
    }
    if (held != 0)
    {
        pressed = true;
        quietFrames = 0;
        for (const SwitchButton button : buttons())
            if ((held & bits(button)) != 0)
                peakBits |= bits(button);
        return std::nullopt;
    }
    if (!pressed || ++quietFrames < graceFrames)
        return std::nullopt;
    return peak();
}

auto ComboCapture::peak() const -> std::vector<std::uint64_t>
{
    std::vector<std::uint64_t> combo;
    for (int bit = 0; bit < 64; ++bit)
        if ((peakBits & (std::uint64_t{ 1 } << bit)) != 0)
            combo.push_back(std::uint64_t{ 1 } << bit);
    return combo;
}

}
