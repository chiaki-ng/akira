#pragma once

#include <array>
#include <cstddef>

#include "platform/pads/pad.hpp"
#include "platform/pads/rumble.hpp"

namespace akira::play
{

class ControllerSettings;

}

namespace akira::platform::hid
{

class HidPad final : public platform::Pad
{
public:
    HidPad(const play::ControllerSettings& controllers, platform::PadRumble& rumble);
    ~HidPad() override;

    HidPad(const HidPad&) = delete;
    HidPad& operator=(const HidPad&) = delete;

    bool poll() override;
    uint64_t heldButtons() const override { return m_buttons; }
    void readTriggers(libchiaking::controllers::ControllerState&) override {}
    HidAnalogStickState stickPos(int index) const override;
    std::optional<HidSixAxisSensorState> readGyro() override;
    void resetMotion() override;
    void sendRumble(float left, float right, float freqLow, float freqHigh) override;

private:
    enum Sensor : std::size_t {
        Handheld,
        FullKey,
        JoyDualLeft,
        JoyDualRight,
        SensorCount,
    };

    const play::ControllerSettings& m_controllers;
    platform::PadRumble& m_rumble;
    PadState m_pad{};
    uint64_t m_buttons = 0;
    std::array<HidSixAxisSensorHandle, SensorCount> m_sixaxis{};
    std::array<bool, SensorCount> m_started{};
};

}
