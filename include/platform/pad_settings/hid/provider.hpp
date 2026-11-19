#pragma once

#include "platform/pads/pad.hpp"
#include "platform/pad_settings/provider.hpp"

namespace akira::platform::hid
{

class HidPadSettingsProvider final : public PadSettingsProvider
{
public:
    explicit HidPadSettingsProvider(const platform::PadDescription& pad);

    static std::string resolveKey(const play::ControllerSettings& settings, bool joycon);

    std::string rumbleKey(play::ControllerSettings& settings) override;
    std::string currentKey(const play::ControllerSettings& settings) const override;
    std::optional<play::PadOutputMode> outputMode(const play::ControllerSettings&) const override { return std::nullopt; }
    void useProfile(const play::RumbleProfile&) override {}
    bool isUnitKey(const std::string&) const override { return false; }
    bool directOutput() const override { return false; }
    bool tunesMotors() const override { return true; }

    bool outputReleased() const override { return false; }
    bool drivesLightbar() const override { return false; }
    void refreshOutputGates() override {}
    std::optional<DirectPreview> previewDirect(const play::RumbleProfile&) override { return std::nullopt; }

private:
    platform::PadDescription m_pad;
};

}
