#include "app/arisa/pad_capture.hpp"

#include <array>
#include <utility>

#include "play/switch_button.hpp"

namespace akira::app::arisa {

namespace provider = ::arisa::provider;

PadCapture::PadCapture(StreamStage& stage)
    : stage(stage)
{
}

void PadCapture::begin()
{
    if (capture)
        return;
    capture.emplace(stage.captureButtons());
}

auto PadCapture::held() -> std::uint64_t
{
    return capture ? bitsOf(capture->held()) : 0;
}

void PadCapture::end()
{
    capture.reset();
}

auto PadCapture::bitsOf(provider::Buttons buttons) -> std::uint64_t
{
    using play::SwitchButton;
    static constexpr std::array<std::pair<provider::Button, SwitchButton>, 16> pairs{ {
        { provider::Button::a, SwitchButton::A },
        { provider::Button::b, SwitchButton::B },
        { provider::Button::x, SwitchButton::X },
        { provider::Button::y, SwitchButton::Y },
        { provider::Button::l, SwitchButton::L },
        { provider::Button::r, SwitchButton::R },
        { provider::Button::zl, SwitchButton::ZL },
        { provider::Button::zr, SwitchButton::ZR },
        { provider::Button::plus, SwitchButton::Plus },
        { provider::Button::minus, SwitchButton::Minus },
        { provider::Button::dpad_up, SwitchButton::Up },
        { provider::Button::dpad_down, SwitchButton::Down },
        { provider::Button::dpad_left, SwitchButton::Left },
        { provider::Button::dpad_right, SwitchButton::Right },
        { provider::Button::left_stick, SwitchButton::StickL },
        { provider::Button::right_stick, SwitchButton::StickR },
    } };
    std::uint64_t mask = 0;
    for (const auto& [button, bit] : pairs)
        if (buttons.contains(button))
            mask |= play::bits(bit);
    return mask;
}

}
