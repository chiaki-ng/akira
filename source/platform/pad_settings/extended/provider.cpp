#include "platform/pad_settings/extended/provider.hpp"

#include <vector>

#include <borealis.hpp>

#include "play/controller_settings.hpp"
#include "platform/pads/extended/ps_output.hpp"
#include "platform/pads/extended/rumble_keys.hpp"
#include "platform/pads/extended/extended_input_manager.hpp"
#include "platform/pad_settings/extended/lightbar_picker.hpp"
#include "settings/views/settings_pad_view.hpp"

using namespace brls::literals;

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

std::optional<std::string> ExtendedPadSettingsProvider::outputSummary(const play::ControllerSettings& settings) const
{
    if (!directOutput())
        return std::nullopt;

    const play::RumbleProfile profile = settings.rumbleProfile(currentKey(settings));
    return profile.output_mode == play::PadOutputMode::Basic
               ? "akira/settings/rumble_output_basic"_i18n
               : "akira/settings/rumble_output_native"_i18n;
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

void ExtendedPadSettingsProvider::addOutputRows(brls::Box& content, play::RumbleProfile& profile,
                                  const Action& store, const Action& reopen)
{
    auto* card = settings::views::SettingsPadView::makeCard();
    card->addView(settings::views::SettingsPadView::makeHeading("akira/settings/rumble_output_heading"_i18n));

    if (m_driver.reason == PadDriverReason::Released) {
        auto* row = new brls::DetailCell();
        row->setMarginLeft(15.0f);
        row->setMarginRight(15.0f);
        row->title->setText("akira/settings/rumble_output_mode"_i18n);
        row->setDetailText("akira/settings/rumble_output_basic"_i18n);
        row->setDetailTextColor(
            brls::Application::getTheme()["brls/text_disabled"]);
        card->addView(row);

        auto* why = settings::views::SettingsPadView::makeHeading("akira/settings/rumble_output_released_hint"_i18n);
        why->setMarginLeft(15.0f);
        why->setMarginRight(15.0f);
        why->setMarginBottom(6.0f);
        card->addView(why);

        content.addView(card);
        return;
    }

    auto* mode = new brls::SelectorCell();
    mode->setMarginLeft(15.0f);
    mode->setMarginRight(15.0f);

    const std::vector<std::string> options = {
        "akira/settings/rumble_output_native"_i18n,
        "akira/settings/rumble_output_basic"_i18n,
    };

    mode->init("akira/settings/rumble_output_mode"_i18n, options,
               static_cast<int>(profile.output_mode),
               [&profile, store, reopen, pad = m_pad, &controllers = m_controllers](int selected) {
                   const auto chosen = selected == 1
                                           ? play::PadOutputMode::Basic
                                           : play::PadOutputMode::Native;
                   if (chosen == profile.output_mode)
                       return;

                   profile.output_mode = chosen;
                   store();

                   ExtendedInputManager::refreshDirectGates(controllers,
                       pad.vendor_id, pad.product_id,
                       pad.has_address ? pad.bt_addr : nullptr);

                   reopen();
               });

    mode->setDetailText(profile.output_mode == play::PadOutputMode::Basic
                            ? "akira/settings/rumble_output_basic_hint"_i18n
                            : "akira/settings/rumble_output_native_hint"_i18n);
    card->addView(mode);

    content.addView(card);
}

void ExtendedPadSettingsProvider::addLightbarRows(brls::Box& content, play::RumbleProfile& profile,
                                    const Action& store, const Action& reopen)
{
    auto* card = settings::views::SettingsPadView::makeCard();
    card->addView(settings::views::SettingsPadView::makeHeading("akira/settings/lightbar_heading"_i18n));

    auto* on = new brls::BooleanCell();
    on->setMarginLeft(15.0f);
    on->setMarginRight(15.0f);
    on->init("akira/settings/lightbar_enabled"_i18n, profile.lightbar_enabled,
             [&profile, store, reopen](bool value) {
                 profile.lightbar_enabled = value;
                 store();
                 reopen();
             });
    card->addView(on);

    if (!profile.lightbar_enabled) {
        content.addView(card);
        return;
    }

    auto* picker = new LightbarPicker(profile.lightbar_r, profile.lightbar_g,
                                      profile.lightbar_b);
    picker->setMarginLeft(15.0f);
    picker->setMarginRight(15.0f);
    picker->setMarginTop(4.0f);

    auto* swatch = new brls::Box();
    swatch->setHeight(30.0f);
    swatch->setMarginLeft(15.0f);
    swatch->setMarginRight(15.0f);
    swatch->setMarginTop(10.0f);
    swatch->setMarginBottom(4.0f);
    swatch->setCornerRadius(6.0f);
    swatch->setBackgroundColor(
        nvgRGB(profile.lightbar_r, profile.lightbar_g, profile.lightbar_b));

    picker->setChangeListener([&profile, store, swatch](std::uint8_t r, std::uint8_t g, std::uint8_t b) {
        profile.lightbar_r = r;
        profile.lightbar_g = g;
        profile.lightbar_b = b;
        swatch->setBackgroundColor(nvgRGB(r, g, b));
        store();
    });

    card->addView(picker);
    card->addView(swatch);

    auto* how = settings::views::SettingsPadView::makeHeading(m_driver.driver == PadDriver::Akira
                                ? "akira/settings/lightbar_hint"_i18n
                                : "akira/settings/lightbar_idle"_i18n);
    how->setMarginLeft(15.0f);
    how->setMarginRight(15.0f);
    how->setMarginBottom(6.0f);
    card->addView(how);

    content.addView(card);
}

bool ExtendedPadSettingsProvider::previewDirect(const play::RumbleProfile& profile)
{
    if (!directOutput() || !m_pad.has_address ||
        profile.output_mode != play::PadOutputMode::Native)
        return false;

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
        return true;

    ExtendedInputManager::writeOutputReportOnce(m_pad.bt_addr, frame, (std::uint16_t)len);

    brls::delay(300, [addr = std::vector<std::uint8_t>(m_pad.bt_addr, m_pad.bt_addr + 6)]() {
        std::uint8_t quiet[kDs5BluetoothFrameBytes];
        const std::size_t n = BuildDs5RumbleFrame(nextPreviewSequence(), 0, 0,
                                                         quiet, sizeof(quiet));
        if (n > 0)
            ExtendedInputManager::writeOutputReportOnce(addr.data(), quiet, (std::uint16_t)n);
    });
    return true;
}

}
