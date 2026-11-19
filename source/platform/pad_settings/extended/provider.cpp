#include "platform/pad_settings/extended/provider.hpp"

#include <vector>

#include "play/controller_settings.hpp"
#include "platform/pads/extended/ps_output.hpp"
#include "platform/pads/extended/rumble_keys.hpp"
#include "platform/pads/extended/extended_input_manager.hpp"

namespace akira::platform::extended
{

ExtendedPadSettingsProvider::ExtendedPadSettingsProvider(const platform::PadDescription& pad,
                                                         const play::ControllerSettings& controllers)
    : m_pad(pad)
    , m_controllers(controllers)
{
}

std::uint8_t ExtendedPadSettingsProvider::nextPreviewSequence()
{
    static std::uint8_t seq = 0;
    return seq++;
}

std::string ExtendedPadSettingsProvider::resolveKey(const play::ControllerSettings& settings, uint16_t vendorId, uint16_t productId,
                                      const uint8_t* address, bool switchNative, bool joycon)
{
    if (address != nullptr) {
        const std::string unit = RumbleKeyForUnit(address);
        if (settings.hasRumbleProfile(unit))
            return unit;
    }

    if (vendorId != 0 || productId != 0) {
        const std::string model = RumbleKeyForModel(vendorId, productId);
        if (settings.hasRumbleProfile(model))
            return model;
    }

    const std::string category =
        RumbleKeyForCategory(PadCategoryFor(vendorId, productId, switchNative, joycon));
    if (settings.hasRumbleProfile(category))
        return category;

    return play::kRumbleKeyDefault;
}

play::RumbleProfile ExtendedPadSettingsProvider::resolveProfile(const play::ControllerSettings& settings, uint16_t vendorId,
                                                   uint16_t productId, const uint8_t* address,
                                                   bool switchNative, bool joycon)
{
    return settings.rumbleProfile(resolveKey(settings, vendorId, productId, address, switchNative, joycon));
}

bool ExtendedPadSettingsProvider::switchNative() const
{
    return m_pad.kind == platform::PadPathKind::JoyCon || m_pad.kind == platform::PadPathKind::SwitchPro;
}

std::string ExtendedPadSettingsProvider::currentKey(const play::ControllerSettings& settings) const
{
    return resolveKey(settings, m_pad.vendor_id, m_pad.product_id,
                      m_pad.has_address ? m_pad.bt_addr : nullptr,
                      switchNative(), m_pad.kind == platform::PadPathKind::JoyCon);
}

std::string ExtendedPadSettingsProvider::rumbleKey(play::ControllerSettings& settings)
{
    if (!switchNative() && (m_pad.vendor_id != 0 || m_pad.product_id != 0)) {
        const play::RumbleProfile current = settings.rumbleProfile(currentKey(settings));
        settings.seedRumbleProfile(
            RumbleKeyForModel(m_pad.vendor_id, m_pad.product_id), &current);
    }

    return currentKey(settings);
}

std::optional<play::PadOutputMode> ExtendedPadSettingsProvider::outputMode(const play::ControllerSettings& settings) const
{
    if (!directOutput())
        return std::nullopt;

    return settings.rumbleProfile(currentKey(settings)).output_mode;
}

void ExtendedPadSettingsProvider::useProfile(const play::RumbleProfile& profile)
{
    PadOutputInputs driver_in;
    driver_in.supported_pad =
        PadTakesDirectOutput(m_pad.vendor_id, m_pad.product_id);
    driver_in.profile_native =
        profile.output_mode == play::PadOutputMode::Native;
    driver_in.backend_enabled   = ExtendedInputManager::backendDirectOutputEnabled();
    driver_in.ownership_refused = ExtendedInputManager::outputOwnershipRefused();

    driver_in.wanted        = true;
    driver_in.address_valid = true;
    driver_in.owns_output   = !driver_in.ownership_refused;

    const PadOutputState resolved = ResolvePadOutput(driver_in);
    m_driver = { resolved.driver, resolved.reason };
}

bool ExtendedPadSettingsProvider::isUnitKey(const std::string& key) const
{
    return RumbleKeyIsUnit(key);
}

bool ExtendedPadSettingsProvider::directOutput() const
{
    return PadTakesDirectOutput(m_pad.vendor_id, m_pad.product_id);
}

bool ExtendedPadSettingsProvider::tunesMotors() const
{
    return m_driver.driver == PadDriver::MissionControl;
}

bool ExtendedPadSettingsProvider::outputReleased() const
{
    return m_driver.reason == PadDriverReason::Released;
}

bool ExtendedPadSettingsProvider::drivesLightbar() const
{
    return m_driver.driver == PadDriver::Akira;
}

void ExtendedPadSettingsProvider::refreshOutputGates()
{
    ExtendedInputManager::refreshDirectGates(m_controllers,
        m_pad.vendor_id, m_pad.product_id,
        m_pad.has_address ? m_pad.bt_addr : nullptr);
}

std::optional<PadSettingsProvider::DirectPreview> ExtendedPadSettingsProvider::previewDirect(const play::RumbleProfile& profile)
{
    if (!directOutput() || !m_pad.has_address ||
        profile.output_mode != play::PadOutputMode::Native)
        return std::nullopt;

    constexpr std::int16_t kPreviewLevel = 6000;
    constexpr std::size_t  kPreviewFrames = 64;

    std::int16_t sample[kPreviewFrames * 2];
    for (std::size_t i = 0; i < kPreviewFrames; i++) {
        sample[i * 2]     = kPreviewLevel;
        sample[i * 2 + 1] = kPreviewLevel;
    }

    const HapticRumble r =
        HapticAudioToRumble(sample, kPreviewFrames, profile.haptic_intensity);

    std::uint8_t frame[kDs5BluetoothFrameBytes];
    const std::size_t len = BuildDs5RumbleFrame(nextPreviewSequence(),
                                                       (std::uint8_t)(r.left >> 8),
                                                       (std::uint8_t)(r.right >> 8),
                                                       frame, sizeof(frame));
    if (len == 0)
        return DirectPreview{};

    ExtendedInputManager::writeOutputReportOnce(m_pad.bt_addr, frame, (std::uint16_t)len);

    return DirectPreview{ .stop = [addr = std::vector<std::uint8_t>(m_pad.bt_addr, m_pad.bt_addr + 6)]() {
        std::uint8_t quiet[kDs5BluetoothFrameBytes];
        const std::size_t n = BuildDs5RumbleFrame(nextPreviewSequence(), 0, 0,
                                                         quiet, sizeof(quiet));
        if (n > 0)
            ExtendedInputManager::writeOutputReportOnce(addr.data(), quiet, (std::uint16_t)n);
    } };
}

}
