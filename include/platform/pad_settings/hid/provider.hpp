#pragma once

#include "platform/pads/pad.hpp"
#include "platform/pad_settings/provider.hpp"

namespace akira::platform::hid
{

class HidPadSettingsProvider final : public PadSettingsProvider
{
public:
    explicit HidPadSettingsProvider(const input::PadDescription& pad);

    static std::string resolveKey(const play::ControllerSettings& settings, bool joycon);

    std::string rumbleKey(play::ControllerSettings& settings) override;
    std::string currentKey(const play::ControllerSettings& settings) const override;
    std::optional<std::string> outputSummary(const play::ControllerSettings&) const override { return std::nullopt; }
    void useProfile(const play::RumbleProfile&) override {}
    bool isUnitKey(const std::string&) const override { return false; }
    bool directOutput() const override { return false; }
    bool tunesMotors() const override { return true; }

    void addOutputRows(brls::Box&, play::RumbleProfile&, const Action&, const Action&) override {}
    void addLightbarRows(brls::Box&, play::RumbleProfile&, const Action&, const Action&) override {}
    bool previewDirect(const play::RumbleProfile&) override { return false; }

private:
    input::PadDescription m_pad;
};

}
