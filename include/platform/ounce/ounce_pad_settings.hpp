#pragma once

#include "input/pad.hpp"
#include "platform/pad_settings.hpp"

namespace akira::platform
{

class OuncePadSettings final : public PadSettings
{
public:
    explicit OuncePadSettings(const input::PadDescription& pad);

    static std::string resolveKey(const SettingsManager& settings, bool joycon);

    std::string rumbleKey(SettingsManager& settings) override;
    std::string currentKey(const SettingsManager& settings) const override;
    std::optional<std::string> outputSummary(const SettingsManager&) const override { return std::nullopt; }
    void useProfile(const input::RumbleProfile&) override {}
    bool isUnitKey(const std::string&) const override { return false; }
    bool directOutput() const override { return false; }
    bool tunesMotors() const override { return true; }

    void addOutputRows(brls::Box&, input::RumbleProfile&, const Action&, const Action&) override {}
    void addLightbarRows(brls::Box&, input::RumbleProfile&, const Action&, const Action&) override {}
    bool previewDirect(const input::RumbleProfile&) override { return false; }

private:
    input::PadDescription m_pad;
};

}
