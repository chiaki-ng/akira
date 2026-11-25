#pragma once

#include <optional>

#include "platform/pads/hid/hid_pad.hpp"
#include "platform/pads/provider.hpp"

namespace akira::platform::hid
{

class HidPadProvider final : public PadProvider
{
public:
    HidPadProvider(const play::ControllerSettings& controllers, platform::PadRumble& rumble);

    void open() override;
    void close() override;
    platform::Pad* acquire(Callbacks& callbacks) override;
    platform::Pad* current() override;
    std::vector<platform::PadDescription> scanPads() const override;
    play::RumbleProfile primaryRumbleProfile() const override;

    void readSecondaryPads(play::InputManager&, std::span<libchiaking::controllers::ControllerInput>, uint8_t&) override {}
    void routeSeatFeedback(uint8_t, const libchiaking::controllers::ControllerFeedback&) override {}
    void fillStreamStats(play::StreamStats&) override {}

private:
    const play::ControllerSettings& m_controllers;
    platform::PadRumble& m_rumble;
    std::optional<HidPad> m_pad;
};

}
