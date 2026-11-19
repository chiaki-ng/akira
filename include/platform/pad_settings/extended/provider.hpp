#pragma once

#include <cstdint>

#include "platform/pads/pad.hpp"
#include "platform/pads/extended/pad_output_state.hpp"
#include "platform/pad_settings/provider.hpp"

namespace akira::platform::extended
{

class ExtendedPadSettingsProvider final : public PadSettingsProvider
{
public:
    ExtendedPadSettingsProvider(const platform::PadDescription& pad, const play::ControllerSettings& controllers);

    static std::string resolveKey(const play::ControllerSettings& settings, uint16_t vendorId, uint16_t productId,
                                  const uint8_t* address, bool switchNative, bool joycon);

    static play::RumbleProfile resolveProfile(const play::ControllerSettings& settings, uint16_t vendorId, uint16_t productId,
                                               const uint8_t* address, bool switchNative, bool joycon);

    std::string rumbleKey(play::ControllerSettings& settings) override;
    std::string currentKey(const play::ControllerSettings& settings) const override;
    std::optional<play::PadOutputMode> outputMode(const play::ControllerSettings& settings) const override;
    void useProfile(const play::RumbleProfile& profile) override;
    bool isUnitKey(const std::string& key) const override;
    bool directOutput() const override;
    bool tunesMotors() const override;

    bool outputReleased() const override;
    bool drivesLightbar() const override;
    void refreshOutputGates() override;
    std::optional<DirectPreview> previewDirect(const play::RumbleProfile& profile) override;

private:
    static std::uint8_t nextPreviewSequence();
    bool switchNative() const;

    platform::PadDescription m_pad;
    const play::ControllerSettings& m_controllers;
    PadDriverState m_driver;
};

}
