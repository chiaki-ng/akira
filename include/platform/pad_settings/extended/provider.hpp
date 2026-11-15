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
    explicit ExtendedPadSettingsProvider(const input::PadDescription& pad);

    static std::string resolveKey(const SettingsManager& settings, uint16_t vendorId, uint16_t productId,
                                  const uint8_t* address, bool switchNative, bool joycon);

    static input::RumbleProfile resolveProfile(const SettingsManager& settings, uint16_t vendorId, uint16_t productId,
                                               const uint8_t* address, bool switchNative, bool joycon);

    std::string rumbleKey(SettingsManager& settings) override;
    std::string currentKey(const SettingsManager& settings) const override;
    std::optional<std::string> outputSummary(const SettingsManager& settings) const override;
    void useProfile(const input::RumbleProfile& profile) override;
    bool isUnitKey(const std::string& key) const override;
    bool directOutput() const override;
    bool tunesMotors() const override;

    void addOutputRows(brls::Box& content, input::RumbleProfile& profile,
                       const Action& store, const Action& reopen) override;
    void addLightbarRows(brls::Box& content, input::RumbleProfile& profile,
                         const Action& store, const Action& reopen) override;
    bool previewDirect(const input::RumbleProfile& profile) override;

private:
    static std::uint8_t nextPreviewSequence();
    bool switchNative() const;

    input::PadDescription m_pad;
    PadDriverState m_driver;
};

}
