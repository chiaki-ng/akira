#pragma once

#include <cstdint>

#include <libchiaking/play_types.hpp>

namespace akira::input {

enum class RemapTarget : std::uint32_t {
    Cross = static_cast<std::uint32_t>(libchiaking::controllers::ControllerButton::cross),
    Moon = static_cast<std::uint32_t>(libchiaking::controllers::ControllerButton::moon),
    Box = static_cast<std::uint32_t>(libchiaking::controllers::ControllerButton::box),
    Pyramid = static_cast<std::uint32_t>(libchiaking::controllers::ControllerButton::pyramid),
    DpadLeft = static_cast<std::uint32_t>(libchiaking::controllers::ControllerButton::dpad_left),
    DpadRight = static_cast<std::uint32_t>(libchiaking::controllers::ControllerButton::dpad_right),
    DpadUp = static_cast<std::uint32_t>(libchiaking::controllers::ControllerButton::dpad_up),
    DpadDown = static_cast<std::uint32_t>(libchiaking::controllers::ControllerButton::dpad_down),
    L1 = static_cast<std::uint32_t>(libchiaking::controllers::ControllerButton::l1),
    R1 = static_cast<std::uint32_t>(libchiaking::controllers::ControllerButton::r1),
    L3 = static_cast<std::uint32_t>(libchiaking::controllers::ControllerButton::l3),
    R3 = static_cast<std::uint32_t>(libchiaking::controllers::ControllerButton::r3),
    Options = static_cast<std::uint32_t>(libchiaking::controllers::ControllerButton::options),
    Share = static_cast<std::uint32_t>(libchiaking::controllers::ControllerButton::share),
    Touchpad = static_cast<std::uint32_t>(libchiaking::controllers::ControllerButton::touchpad),
    Ps = static_cast<std::uint32_t>(libchiaking::controllers::ControllerButton::ps),
    L2 = 1u << libchiaking::controllers::ControllerState::button_count,
    R2 = 1u << (libchiaking::controllers::ControllerState::button_count + 1),
};

constexpr auto remapKey(RemapTarget target) noexcept -> std::uint32_t
{
    return static_cast<std::uint32_t>(target);
}

}
