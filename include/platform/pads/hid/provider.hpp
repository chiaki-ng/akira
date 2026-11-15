#pragma once

#include <optional>

#include "platform/pads/hid/hid_pad.hpp"
#include "platform/pads/provider.hpp"

namespace akira::platform::hid
{

class HidPadProvider final : public PadProvider
{
public:
    void open() override;
    void close() override;
    input::Pad* acquire(Callbacks& callbacks) override;
    input::Pad* current() override;
    std::vector<input::PadDescription> scanPads() const override;
    input::RumbleProfile primaryRumbleProfile() const override;

    void readSecondaryPads(InputManager&, std::span<libchiaking::controllers::ControllerInput>, uint8_t&) override {}
    void routeSeatFeedback(uint8_t, const libchiaking::controllers::ControllerFeedback&) override {}
    void fillStreamStats(StreamStats&) override {}

private:
    std::optional<HidPad> m_pad;
};

}
