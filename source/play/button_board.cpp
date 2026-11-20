#include "play/button_board.hpp"

#include <algorithm>
#include <utility>

#include "play/controller_settings.hpp"
#include "play/remap_target.hpp"
#include "play/swipe_direction.hpp"
#include "play/switch_button.hpp"

namespace akira::play {

ButtonBoard::ButtonBoard(ControllerSettings& settings)
    : settings(&settings)
{
}

auto ButtonBoard::targets() -> const std::vector<Target>&
{
    static const std::vector<Target> all{
        { remapKey(RemapTarget::L2), "akira/remap/l2", "img/ps5/l2.png", Side::Leading },
        { remapKey(RemapTarget::L1), "akira/remap/l1", "img/ps5/l1.png", Side::Leading },
        { remapKey(RemapTarget::Share), "akira/remap/share", "img/ps5/share.png", Side::Leading },
        { remapKey(RemapTarget::L3), "akira/remap/l3", "img/ps5/l3.png", Side::Leading },
        { remapKey(RemapTarget::Ps), "akira/remap/ps", "img/ps5/ps.png", Side::Leading },
        { remapKey(RemapTarget::R3), "akira/remap/r3", "img/ps5/r3.png", Side::Leading },
        { remapKey(RemapTarget::R2), "akira/remap/r2", "img/ps5/r2.png", Side::Trailing },
        { remapKey(RemapTarget::R1), "akira/remap/r1", "img/ps5/r1.png", Side::Trailing },
        { remapKey(RemapTarget::Options), "akira/remap/options", "img/ps5/options.png", Side::Trailing },
        { remapKey(RemapTarget::Pyramid), "akira/remap/triangle", "img/ps5/triangle.png", Side::Trailing },
        { remapKey(RemapTarget::Box), "akira/remap/square", "img/ps5/square.png", Side::Trailing },
        { remapKey(RemapTarget::Moon), "akira/remap/circle", "img/ps5/circle.png", Side::Trailing },
        { remapKey(RemapTarget::Cross), "akira/remap/cross", "img/ps5/cross.png", Side::Trailing },
        { remapKey(RemapTarget::Touchpad), "akira/remap/touchpad", "img/ps5/touchpad.png", Side::Touchpad },
        { SWIPE_TOUCHPAD_UP, "akira/remap/swipe_up", "img/ps5/swipe_up.png", Side::Touchpad },
        { SWIPE_TOUCHPAD_DOWN, "akira/remap/swipe_down", "img/ps5/swipe_down.png", Side::Touchpad },
        { SWIPE_TOUCHPAD_LEFT, "akira/remap/swipe_left", "img/ps5/swipe_left.png", Side::Touchpad },
        { SWIPE_TOUCHPAD_RIGHT, "akira/remap/swipe_right", "img/ps5/swipe_right.png", Side::Touchpad },
    };
    return all;
}

auto ButtonBoard::label(std::uint64_t button) -> std::string_view
{
    switch (static_cast<SwitchButton>(button))
    {
        case SwitchButton::A: return "A";
        case SwitchButton::B: return "B";
        case SwitchButton::X: return "X";
        case SwitchButton::Y: return "Y";
        case SwitchButton::L: return "L";
        case SwitchButton::R: return "R";
        case SwitchButton::ZL: return "ZL";
        case SwitchButton::ZR: return "ZR";
        case SwitchButton::Plus: return "Plus";
        case SwitchButton::Minus: return "Minus";
        case SwitchButton::StickL: return "L Stick";
        case SwitchButton::StickR: return "R Stick";
        case SwitchButton::LeftSL: return "SL(L)";
        case SwitchButton::LeftSR: return "SR(L)";
        case SwitchButton::RightSL: return "SL(R)";
        case SwitchButton::RightSR: return "SR(R)";
        case SwitchButton::Up: return "D-Up";
        case SwitchButton::Down: return "D-Down";
        case SwitchButton::Left: return "D-Left";
        case SwitchButton::Right: return "D-Right";
        case SwitchButton::StickLUp: return "LS-Up";
        case SwitchButton::StickLDown: return "LS-Down";
        case SwitchButton::StickLLeft: return "LS-Left";
        case SwitchButton::StickLRight: return "LS-Right";
        case SwitchButton::StickRUp: return "RS-Up";
        case SwitchButton::StickRDown: return "RS-Down";
        case SwitchButton::StickRLeft: return "RS-Left";
        case SwitchButton::StickRRight: return "RS-Right";
    }
    return "?";
}

auto ButtonBoard::image(std::uint64_t button) -> std::string_view
{
    switch (static_cast<SwitchButton>(button))
    {
        case SwitchButton::A: return "img/buttons/a.png";
        case SwitchButton::B: return "img/buttons/b.png";
        case SwitchButton::X: return "img/buttons/x.png";
        case SwitchButton::Y: return "img/buttons/y.png";
        case SwitchButton::L: return "img/buttons/l.png";
        case SwitchButton::R: return "img/buttons/r.png";
        case SwitchButton::ZL: return "img/buttons/zl.png";
        case SwitchButton::ZR: return "img/buttons/zr.png";
        case SwitchButton::Plus: return "img/buttons/plus.png";
        case SwitchButton::Minus: return "img/buttons/minus.png";
        case SwitchButton::StickL: return "img/buttons/lstick_click.png";
        case SwitchButton::StickR: return "img/buttons/rstick_click.png";
        case SwitchButton::LeftSL:
        case SwitchButton::RightSL: return "img/buttons/sl.png";
        case SwitchButton::LeftSR:
        case SwitchButton::RightSR: return "img/buttons/sr.png";
        case SwitchButton::Up: return "img/buttons/dpad_up.png";
        case SwitchButton::Down: return "img/buttons/dpad_down.png";
        case SwitchButton::Left: return "img/buttons/dpad_left.png";
        case SwitchButton::Right: return "img/buttons/dpad_right.png";
        case SwitchButton::StickLUp: return "img/buttons/lstick_up.png";
        case SwitchButton::StickLDown: return "img/buttons/lstick_down.png";
        case SwitchButton::StickLLeft: return "img/buttons/lstick_left.png";
        case SwitchButton::StickLRight: return "img/buttons/lstick_right.png";
        case SwitchButton::StickRUp: return "img/buttons/rstick_up.png";
        case SwitchButton::StickRDown: return "img/buttons/rstick_down.png";
        case SwitchButton::StickRLeft: return "img/buttons/rstick_left.png";
        case SwitchButton::StickRRight: return "img/buttons/rstick_right.png";
    }
    return {};
}

auto ButtonBoard::sorted(std::vector<std::uint64_t> buttons) -> std::vector<std::uint64_t>
{
    std::ranges::sort(buttons);
    return buttons;
}

auto ButtonBoard::mapping(std::uint32_t key) const -> std::vector<std::uint64_t>
{
    const ButtonMapping& current = settings->buttonMapping();
    const auto found = current.find(key);
    return found == current.end() ? std::vector<std::uint64_t>{} : found->second;
}

auto ButtonBoard::isDefault(std::uint32_t key) const -> bool
{
    const ButtonMapping defaults = ControllerSettings::defaultButtonMapping();
    const auto found = defaults.find(key);
    const std::vector<std::uint64_t> wanted = found == defaults.end() ? std::vector<std::uint64_t>{} : found->second;
    return sorted(wanted) == sorted(mapping(key));
}

auto ButtonBoard::conflicts(std::uint32_t key) const -> bool
{
    const std::vector<std::uint64_t> mine = sorted(mapping(key));
    if (mine.empty())
        return false;
    return std::ranges::any_of(settings->buttonMapping(), [&](const auto& entry) {
        return entry.first != key && !entry.second.empty() && sorted(entry.second) == mine;
    });
}

void ButtonBoard::assign(std::uint32_t key, std::vector<std::uint64_t> combo)
{
    if (combo.empty())
        return;
    ButtonMapping changed = settings->buttonMapping();
    changed[key] = std::move(combo);
    settings->setButtonMapping(changed);
}

void ButtonBoard::reset(std::uint32_t key)
{
    const ButtonMapping defaults = ControllerSettings::defaultButtonMapping();
    const auto found = defaults.find(key);
    ButtonMapping changed = settings->buttonMapping();
    changed[key] = found == defaults.end() ? std::vector<std::uint64_t>{} : found->second;
    settings->setButtonMapping(changed);
}

void ButtonBoard::resetAll()
{
    settings->setButtonMapping(ControllerSettings::defaultButtonMapping());
}

}
