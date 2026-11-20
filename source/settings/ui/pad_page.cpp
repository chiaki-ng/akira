#include "settings/ui/pad_page.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/controls/color_picker.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <utility>

#include "play/controller_settings.hpp"

namespace akira::settings::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

PadPage::PadPage(SettingsModel& model, const HelpCatalog& help, const app::Strings& strings)
    : model(&model)
    , help(&help)
    , strings(&strings)
{
}

auto PadPage::edit(std::size_t index, const PadBench::Pad& pad) const -> std::function<void(Change)>
{
    play::ControllerSettings& controllers = model->parts().controllers;
    PadBench& bench = model->parts().pads;
    return [&controllers, &bench, index, key = pad.key](Change change) {
        play::RumbleProfile profile = controllers.rumbleProfile(key);
        change(profile);
        controllers.setRumbleProfile(key, profile);
        bench.use(index, profile);
    };
}

auto PadPage::body(composition::Context& context) const -> composition::Element
{
    const SettingsModel::Status& status = context.read(model->view());
    if (status.pad >= status.pads.size())
        return components::Text(strings->text("akira/settings/connected_controllers_none"));
    const std::size_t index = status.pad;
    const PadBench::Pad& pad = status.pads[index];
    const play::RumbleProfile profile = model->parts().controllers.rumbleProfile(pad.key);
    const SettingRows rows(*model, *help, *strings);
    const auto change = edit(index, pad);
    SettingsModel* held = model;
    play::ControllerSettings& controllers = model->parts().controllers;
    std::vector<composition::Element> sections;
    if (pad.direct)
        sections.push_back(output(rows, index, pad, profile));
    sections.push_back(rows.section(rows.text("akira/settings/rumble_source_heading"), rumble(rows, pad, change)));
    if (std::vector<composition::Element> shaped = shaping(rows, pad, profile, change); !shaped.empty())
        sections.push_back(rows.section(rows.text("akira/settings/rumble_envelope_heading"), std::move(shaped)));
    if (pad.direct)
        sections.push_back(lightbar(rows, pad, profile, change));
    if (pad.joyCon)
        sections.push_back(rows.section(std::string(),
            rows.picker<play::GyroSource>(rows.text("akira/settings/gyro_source"), "",
                { { play::GyroSource::Auto, rows.text("akira/settings/gyro_auto") },
                    { play::GyroSource::Left, rows.text("akira/settings/gyro_left") },
                    { play::GyroSource::Right, rows.text("akira/settings/gyro_right") } },
                [&controllers] { return controllers.gyroSource(); },
                [&controllers](play::GyroSource source) { controllers.setGyroSource(source); })));
    PadBench& bench = model->parts().pads;
    sections.push_back(rows.section(std::string(),
        components::HStack(components::HStackOptions{ .spacing = theme::space::md },
            components::Button(rows.text("akira/settings/rumble_test"), components::Image::SystemName{ "gamepad" },
                [&bench, &controllers, index, key = pad.key] { bench.buzz(index, controllers.rumbleProfile(key)); })
                .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                .id(composition::ElementKey{ "pad.buzz" }),
            components::Button(rows.text("akira/settings/rumble_reset"), components::Image::SystemName{ "restart_alt" },
                components::ButtonRole::destructive,
                [held, &bench, &controllers, index, key = pad.key] {
                    controllers.resetRumbleProfile(key);
                    bench.use(index, controllers.rumbleProfile(key));
                    held->commit();
                })
                .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                .id(composition::ElementKey{ "pad.reset" }))));
    return structure::Form(std::move(sections)).on_appear([&bench, index, profile] { bench.use(index, profile); });
}

auto PadPage::output(const SettingRows& rows, std::size_t index, const PadBench::Pad& pad,
    const play::RumbleProfile& profile) const -> composition::Element
{
    if (pad.outputReleased)
        return rows.section(rows.text("akira/settings/rumble_output_heading"),
            rows.value(rows.text("akira/settings/rumble_output_mode"), rows.text("akira/settings/rumble_output_basic")),
                rows.note(rows.text("akira/settings/rumble_output_released_hint"), false));
    PadBench& bench = model->parts().pads;
    SettingsModel* held = model;
    const auto change = edit(index, pad);
    return rows.section(rows.text("akira/settings/rumble_output_heading"),
        rows.picker<play::PadOutputMode>(rows.text("akira/settings/rumble_output_mode"), "",
            { { play::PadOutputMode::Native, rows.text("akira/settings/rumble_output_native") },
                { play::PadOutputMode::Basic, rows.text("akira/settings/rumble_output_basic") } },
            [mode = profile.output_mode] { return mode; },
            [change, &bench, held, index](play::PadOutputMode chosen) {
                change([chosen](play::RumbleProfile& edited) { edited.output_mode = chosen; });
                bench.refreshOutputGates(index);
                held->scanPads();
            }));
}

auto PadPage::rumble(const SettingRows& rows, const PadBench::Pad& pad, const std::function<void(Change)>& change) const
    -> std::vector<composition::Element>
{
    play::ControllerSettings& controllers = model->parts().controllers;
    std::vector<composition::Element> shown;
    if (!pad.direct)
        shown.push_back(rows.picker<play::RumbleSource>(rows.text("akira/settings/rumble_source"), "settings/rumbleSource",
            { { play::RumbleSource::Off, rows.text("akira/settings/rumble_source_off") },
                { play::RumbleSource::Derived, rows.text("akira/settings/rumble_source_derived") },
                { play::RumbleSource::Game, rows.text("akira/settings/rumble_source_game") } },
            [&controllers, key = pad.key] { return controllers.rumbleProfile(key).rumble_source; },
            [change](play::RumbleSource chosen) { change([chosen](play::RumbleProfile& p) { p.rumble_source = chosen; }); }));
    shown.push_back(rows.picker<play::HapticIntensity>(rows.text("akira/settings/haptic_intensity"), "settings/hapticIntensity",
        { { play::HapticIntensity::Off, rows.text("akira/settings/haptic_intensity_off") },
            { play::HapticIntensity::VeryWeak, rows.text("akira/settings/haptic_intensity_very_weak") },
            { play::HapticIntensity::Weak, rows.text("akira/settings/haptic_intensity_weak") },
            { play::HapticIntensity::Normal, rows.text("akira/settings/haptic_intensity_normal") },
            { play::HapticIntensity::Strong, rows.text("akira/settings/haptic_intensity_strong") },
            { play::HapticIntensity::VeryStrong, rows.text("akira/settings/haptic_intensity_very_strong") } },
        [&controllers, key = pad.key] { return controllers.rumbleProfile(key).haptic_intensity; },
        [change](play::HapticIntensity chosen) { change([chosen](play::RumbleProfile& p) { p.haptic_intensity = chosen; }); }));
    return shown;
}

auto PadPage::shaping(const SettingRows& rows, const PadBench::Pad& pad, const play::RumbleProfile& profile,
    const std::function<void(Change)>& change) const -> std::vector<composition::Element>
{
    play::ControllerSettings& controllers = model->parts().controllers;
    const std::string key = pad.key;
    auto read = [&controllers, key](float play::RumbleProfile::* field) {
        return [&controllers, key, field] { return static_cast<double>(controllers.rumbleProfile(key).*field); };
    };
    auto unit = [](double low, double high) { return controls::ClosedRange<double>{ .lower_bound = low, .upper_bound = high }; };
    std::vector<composition::Element> shown;
    if (pad.tunesMotors)
    {
        shown.push_back(rows.slider(rows.text("akira/settings/rumble_strength"), "", unit(0.0, 1.0), 0.01,
            read(&play::RumbleProfile::strength),
            [change](double value) { change([value](play::RumbleProfile& p) { p.strength = static_cast<float>(value); }); },
            std::format("{:.2f}", profile.strength)));
        shown.push_back(rows.slider(rows.text("akira/settings/rumble_ceiling"), "", unit(0.0, 1.0), 0.01,
            read(&play::RumbleProfile::ceiling),
            [change](double value) { change([value](play::RumbleProfile& p) { p.ceiling = static_cast<float>(value); }); },
            std::format("{:.2f}", profile.ceiling)));
        if (!profile.per_motor && !pad.direct)
        {
            shown.push_back(rows.slider(rows.text("akira/settings/rumble_low_freq"), "", unit(40.0, 320.0), 5.0,
                read(&play::RumbleProfile::freq_low),
                [change](double value) { change([value](play::RumbleProfile& p) { p.freq_low = std::floor(static_cast<float>(value)); }); },
                strings->format("akira/settings/hz_format", static_cast<int>(profile.freq_low))));
            shown.push_back(rows.slider(rows.text("akira/settings/rumble_high_freq"), "", unit(40.0, 320.0), 5.0,
                read(&play::RumbleProfile::freq_high),
                [change](double value) { change([value](play::RumbleProfile& p) { p.freq_high = std::floor(static_cast<float>(value)); }); },
                strings->format("akira/settings/hz_format", static_cast<int>(profile.freq_high))));
        }
    }
    if (!pad.direct)
    {
        auto percent = [](double value) { return std::floor(static_cast<float>(value) * 100.0F) / 100.0F; };
        shown.push_back(rows.slider(rows.text("akira/settings/rumble_attack"), "", unit(0.20, 1.0), 0.01,
            read(&play::RumbleProfile::envelope_attack),
            [change, percent](double value) { change([&](play::RumbleProfile& p) { p.envelope_attack = percent(value); }); },
            strings->format("akira/settings/percent_format", static_cast<int>(profile.envelope_attack * 100.0F))));
        shown.push_back(rows.slider(rows.text("akira/settings/rumble_sustain"), "", unit(0.50, 0.95), 0.01,
            read(&play::RumbleProfile::envelope_decay),
            [change, percent](double value) { change([&](play::RumbleProfile& p) { p.envelope_decay = percent(value); }); },
            strings->format("akira/settings/percent_format", static_cast<int>(profile.envelope_decay * 100.0F))));
    }
    return shown;
}

auto PadPage::lightbar(const SettingRows& rows, const PadBench::Pad& pad, const play::RumbleProfile& profile,
    const std::function<void(Change)>& change) const -> composition::Element
{
    play::ControllerSettings& controllers = model->parts().controllers;
    SettingsModel* held = model;
    const std::string key = pad.key;
    std::vector<composition::Element> shown;
    shown.push_back(rows.toggle(rows.text("akira/settings/lightbar_enabled"), "",
        [&controllers, key] { return controllers.rumbleProfile(key).lightbar_enabled; },
        [change](bool value) { change([value](play::RumbleProfile& p) { p.lightbar_enabled = value; }); }));
    if (profile.lightbar_enabled)
    {
        shown.push_back(controls::ColorPicker(rows.text("akira/settings/lightbar_heading"),
            composition::Binding<theme::Colour>(
                [&controllers, key] {
                    const play::RumbleProfile current = controllers.rumbleProfile(key);
                    return theme::Colour::hex((std::uint32_t{ current.lightbar_r } << 16U) | (std::uint32_t{ current.lightbar_g } << 8U)
                        | std::uint32_t{ current.lightbar_b });
                },
                [change, held](theme::Colour colour) {
                    change([colour](play::RumbleProfile& p) {
                        p.lightbar_r = static_cast<std::uint8_t>(std::lround(std::clamp(colour.red, 0.0F, 1.0F) * 255.0F));
                        p.lightbar_g = static_cast<std::uint8_t>(std::lround(std::clamp(colour.green, 0.0F, 1.0F) * 255.0F));
                        p.lightbar_b = static_cast<std::uint8_t>(std::lround(std::clamp(colour.blue, 0.0F, 1.0F) * 255.0F));
                    });
                    held->commit();
                }),
            false)
                           .id(composition::ElementKey{ "pad.lightbar" }));
        if (!pad.drivesLightbar)
            shown.push_back(rows.note(rows.text("akira/settings/lightbar_idle"), false));
    }
    return rows.section(rows.text("akira/settings/lightbar_heading"), std::move(shown));
}

}
