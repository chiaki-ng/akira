#include "settings/ui/general_section.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/provider/surface/display_mode.hpp>
#include <arisa/structure/appearance_settings_section.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/theme/dynamic_type_size.hpp>

#include <array>
#include <cmath>
#include <format>
#include <utility>
#include <vector>

#include "app/app_settings.hpp"
#include "app/notifier.hpp"
#include "app/version.hpp"
#include "network/network_settings.hpp"
#include "play/stream_settings.hpp"

namespace akira::settings::ui {

namespace appearance = ::arisa::appearance;
namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace provider = ::arisa::provider;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

GeneralSection::GeneralSection(SettingsModel& model, const HelpCatalog& help, const app::Strings& strings)
    : model(&model)
    , help(&help)
    , strings(&strings)
{
}

auto GeneralSection::body(composition::Context& context) const -> composition::Element
{
    static_cast<void>(context.read(model->view()));
    const SettingRows rows(*model, *help, *strings);
    std::vector<composition::Element> sections;
    const std::optional<appearance::AppearanceModel>& found = context.environment(components::Environment::appearance);
    if (found)
    {
        static_cast<void>(context.read(found->settings()));
        sections.push_back(appearance(*found, rows));
        sections.push_back(display(*found, rows));
        sections.push_back(accessibility(*found, rows));
    }
    sections.push_back(language(rows));
    sections.push_back(performance(rows));
    sections.push_back(about(rows));
    return structure::Form(std::move(sections));
}

auto GeneralSection::themeLabel(const std::string& pack) const -> std::string
{
    constexpr std::string_view prefix = "akira-";
    if (!pack.starts_with(prefix))
        return pack;
    const std::string key = "akira/settings/theme_" + pack.substr(prefix.size());
    const std::string shown = strings->text(key);
    return shown == key ? pack : shown;
}

auto GeneralSection::appearance(const appearance::AppearanceModel& look, const SettingRows& rows) const
    -> composition::Element
{
    using Overrides = appearance::AppearanceOverrides;
    std::vector<std::pair<std::string, std::string>> packs;
    for (const std::string& name : look.theme_names())
        packs.emplace_back(name, themeLabel(name));
    std::vector<composition::Element> shown;
    shown.push_back(rows.picker<std::string>(rows.text("akira/settings/theme"), "settings/theme", std::move(packs),
        [look] { return look.settings().get().theme; },
        [look](std::string chosen) { look.update([&chosen](Overrides& o) { o.theme = chosen; }); }));
    shown.push_back(rows.picker<appearance::ColorSchemePreference>(rows.text("akira/settings/scheme"), "",
        { { appearance::ColorSchemePreference::system, rows.text("akira/settings/scheme_system") },
            { appearance::ColorSchemePreference::light, rows.text("akira/settings/scheme_light") },
            { appearance::ColorSchemePreference::dark, rows.text("akira/settings/scheme_dark") } },
        [look] { return look.settings().get().color_scheme; },
        [look](appearance::ColorSchemePreference chosen) {
            look.update([chosen](Overrides& o) { o.color_scheme = chosen; });
        })
                        .picker_style(controls::PickerStyle::segmented()));
    app::AppSettings& general = model->parts().app;
    shown.push_back(rows.picker<app::RailEdge>(rows.text("akira/settings/rail_edge"), "",
        { { app::RailEdge::Left, rows.text("akira/settings/rail_edge_left") },
            { app::RailEdge::Top, rows.text("akira/settings/rail_edge_top") },
            { app::RailEdge::Right, rows.text("akira/settings/rail_edge_right") },
            { app::RailEdge::Bottom, rows.text("akira/settings/rail_edge_bottom") } },
        [&general] { return general.railEdge(); }, [&general](app::RailEdge chosen) { general.setRailEdge(chosen); })
                        .picker_style(controls::PickerStyle::segmented()));
    shown.push_back(accent(look));
    constexpr std::array sizes{ theme::DynamicTypeSize::x_small, theme::DynamicTypeSize::small,
        theme::DynamicTypeSize::medium, theme::DynamicTypeSize::large, theme::DynamicTypeSize::x_large,
        theme::DynamicTypeSize::xx_large, theme::DynamicTypeSize::xxx_large };
    constexpr std::array sizeKeys{ "akira/settings/text_size_xs", "akira/settings/text_size_s",
        "akira/settings/text_size_m", "akira/settings/text_size_l", "akira/settings/text_size_xl",
        "akira/settings/text_size_xxl", "akira/settings/text_size_xxxl" };
    std::vector<std::pair<theme::DynamicTypeSize, std::string>> sized;
    for (std::size_t index = 0; index < sizes.size(); ++index)
        sized.emplace_back(sizes[index], rows.text(sizeKeys[index]));
    shown.push_back(rows.picker<theme::DynamicTypeSize>(rows.text("akira/settings/text_size"), "", std::move(sized),
        [look] { return look.settings().get().dynamic_type_size; },
        [look](theme::DynamicTypeSize chosen) { look.update([chosen](Overrides& o) { o.dynamic_type_size = chosen; }); }));
    return rows.section(rows.text("akira/settings/appearance"), std::move(shown));
}

auto GeneralSection::accent(const appearance::AppearanceModel& look) const -> composition::Element
{
    using Section = structure::AppearanceSettingsSection;
    constexpr std::array names{ "akira/settings/accent_blue", "akira/settings/accent_purple",
        "akira/settings/accent_pink", "akira/settings/accent_red", "akira/settings/accent_orange",
        "akira/settings/accent_yellow", "akira/settings/accent_green", "akira/settings/accent_graphite" };
    std::vector<std::pair<std::size_t, std::string>> labels{ { Section::theme_accent, strings->text("akira/settings/accent_theme") } };
    for (std::size_t index = 0; index < Section::accents.size(); ++index)
        labels.emplace_back(index, strings->text(names[index]));
    SettingsModel* edit = model;
    return controls::Picker(strings->text("akira/settings/accent"),
        composition::Binding<std::size_t>([look] { return Section::accent_index(look.settings().get()); },
            [look, edit](std::size_t index) {
                look.update([index](appearance::AppearanceOverrides& o) {
                    o.accent = index < Section::accents.size() ? std::optional{ Section::accents[index].colour } : std::nullopt;
                });
                edit->commit();
            }),
        [labels] {
            std::vector<composition::Element> items;
            for (const auto& [index, label] : labels)
            {
                composition::Element item = components::Text(label);
                if (index < Section::accents.size())
                    item = std::move(item).foreground_style(Section::accents[index].colour);
                items.push_back(std::move(item).tag(index));
            }
            return composition::Element(components::Group(std::move(items)));
        })
        .id(composition::ElementKey{ "general.accent" });
}

auto GeneralSection::display(const appearance::AppearanceModel& look, const SettingRows& rows) const
    -> composition::Element
{
    auto scale = [look, &rows, this](provider::DisplayMode mode, const char* key) {
        const float current = look.settings().get().ui_scale.resolve(mode);
        return rows.slider(rows.text(key), "",
            controls::ClosedRange<double>{ .lower_bound = appearance::UiScales::minimum * 100.0,
                .upper_bound = appearance::UiScales::maximum * 100.0 },
            5.0, [look, mode] { return std::round(look.settings().get().ui_scale.resolve(mode) * 100.0); },
            [look, mode](double value) {
                look.update([mode, value](appearance::AppearanceOverrides& o) {
                    o.ui_scale = o.ui_scale.with(mode, static_cast<float>(value / 100.0));
                });
            },
            strings->format("akira/settings/percent_format", static_cast<int>(std::lround(current * 100.0F))));
    };
    return rows.section(rows.text("akira/settings/display"),
        scale(provider::DisplayMode::handheld, "akira/settings/scale_handheld"),
            scale(provider::DisplayMode::docked, "akira/settings/scale_docked"));
}

auto GeneralSection::accessibility(const appearance::AppearanceModel& look, const SettingRows& rows) const
    -> composition::Element
{
    using Overrides = appearance::AppearanceOverrides;
    using Settings = appearance::AppearanceSettings;
    auto flag = [look, &rows](const char* key, bool Settings::* field, std::optional<bool> Overrides::* slot) {
        return rows.toggle(rows.text(key), "", [look, field] { return look.settings().get().*field; },
            [look, slot](bool value) { look.update([slot, value](Overrides& o) { o.*slot = value; }); });
    };
    return rows.section(rows.text("akira/settings/accessibility"),
        flag("akira/settings/reduce_motion", &Settings::reduce_motion, &Overrides::reduce_motion),
            flag("akira/settings/reduce_transparency", &Settings::reduce_transparency, &Overrides::reduce_transparency),
            flag("akira/settings/increase_contrast", &Settings::increase_contrast, &Overrides::increase_contrast),
            flag("akira/settings/bold_text", &Settings::bold_text, &Overrides::bold_text));
}

auto GeneralSection::language(const SettingRows& rows) const -> composition::Element
{
    app::AppSettings& app = model->parts().app;
    std::vector<composition::Element> shown;
    shown.push_back(rows.picker<std::string>(rows.text("akira/settings/language"), "settings/language",
        { { std::string(), rows.text("akira/settings/lang_system") }, { "en-US", rows.text("akira/settings/lang_en") },
            { "zh-Hans", rows.text("akira/settings/lang_zh_hans") } },
        [&app] {
            const std::string& locale = app.debugLocale();
            return locale == "en-US" || locale == "zh-Hans" ? locale : std::string();
        },
        [&app](std::string chosen) { app.setDebugLocale(chosen); }));
    shown.push_back(rows.toggle(rows.text("akira/settings/connection_stages"), "settings/connectionShowStages",
        [&app] { return app.connectionShowStages(); }, [&app](bool value) { app.setConnectionShowStages(value); }));
    return rows.section(rows.text("akira/settings/language"), std::move(shown));
}

auto GeneralSection::performance(const SettingRows& rows) const -> composition::Element
{
    network::NetworkSettings& network = model->parts().network;
    play::StreamSettings& stream = model->parts().stream;
    std::vector<composition::Element> shown;
    shown.push_back(rows.toggle(rows.text("akira/settings/holepunch_retry"), "settings/holepunchRetry",
        [&network] { return network.holepunchRetry(); }, [&network](bool value) { network.setHolepunchRetry(value); }));
    shown.push_back(rows.toggle(rows.text("akira/settings/request_idr"), "settings/requestIdrOnFecFailure",
        [&stream] { return stream.requestIdrOnFecFailure(); },
        [&stream](bool value) { stream.setRequestIdrOnFecFailure(value); }));
    shown.push_back(rows.slider(rows.text("akira/settings/packet_loss_max"), "settings/packetLossMax",
        controls::ClosedRange<double>{ .lower_bound = 0.0, .upper_bound = 1.0 }, 0.05,
        [&stream] { return static_cast<double>(stream.packetLossMax()); },
        [&stream](double value) { stream.setPacketLossMax(static_cast<float>(value)); },
        strings->format("akira/settings/percent_format", static_cast<int>(stream.packetLossMax() * 100.0F))));
    return rows.section(rows.text("akira/settings/performance"), std::move(shown));
}

auto GeneralSection::about(const SettingRows& rows) const -> composition::Element
{
    SettingsModel* edit = model;
    std::string unlocked = rows.text("akira/settings/power_user_unlocked");
    return rows.section(std::string(),
        components::Button([edit, unlocked] {
            if (edit->tapVersion())
                edit->parts().notifier.notify(unlocked);
        },
              components::LabeledContent(rows.text("akira/settings/version"), std::string("Akira ") + app::version::string()))
                .help(help->title("settings/version"), help->body("settings/version"))
                .id(composition::ElementKey{ "settings/version" }));
}

}
