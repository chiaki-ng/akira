#pragma once

#include <optional>

#include "platform/pads/hid/hid_pad.hpp"
#include "platform/pads/provider.hpp"

namespace akira::platform::hid
{

class HidPadProvider final : public PadProvider
{
public:
    explicit HidPadProvider(const play::ControllerSettings& controllers);

    void open() override;
    void close() override;
    input::Pad* acquire(Callbacks& callbacks) override;
    input::Pad* current() override;
    std::vector<input::PadDescription> scanPads() const override;
    play::RumbleProfile primaryRumbleProfile() const override;

    void readSecondaryPads(play::InputManager&, std::span<libchiaking::controllers::ControllerInput>, uint8_t&) override {}
    void routeSeatFeedback(uint8_t, const libchiaking::controllers::ControllerFeedback&) override {}
    void fillStreamStats(play::StreamStats&) override {}

private:
    const play::ControllerSettings& m_controllers;
    std::optional<HidPad> m_pad;
};

}
