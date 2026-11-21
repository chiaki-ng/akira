#include "settings/ui/setting_rows.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_sizing.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>
#include <arisa/structure/disclosure_row.hpp>

namespace akira::settings::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

SettingRows::SettingRows(SettingsModel& model, const HelpCatalog& help, const app::Strings& strings)
    : model(&model)
    , help(&help)
    , strings(&strings)
{
}

auto SettingRows::rowKey(std::string_view label, std::string_view help) -> std::string
{
    return help.empty() ? std::string(label) : std::string(help);
}

auto SettingRows::described(composition::Element row, std::string_view id, const std::string& key) const
    -> composition::Element
{
    if (explained(id))
        row = std::move(row).help(help->title(id), help->body(id));
    return std::move(row).id(composition::ElementKey{ key });
}

auto SettingRows::toggle(std::string label, std::string_view id, std::function<bool()> read,
    std::function<void(bool)> write) const -> composition::Element
{
    SettingsModel* edit = model;
    const std::string key = rowKey(label, id);
    return described(controls::Toggle(std::move(label),
                         composition::Binding<bool>(std::move(read),
                             [edit, write = std::move(write)](bool value) {
                                 write(value);
                                 edit->commit();
                             })),
        id, key);
}

auto SettingRows::slider(std::string label, std::string_view id, controls::ClosedRange<double> range, double step,
    std::function<double()> read, std::function<void(double)> write, std::string shown) const -> composition::Element
{
    SettingsModel* edit = model;
    const std::string key = rowKey(label, id);
    return described(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                            .spacing = theme::space::xs },
                         components::HStack(components::Text(label), components::Spacer(),
                             components::Text(std::move(shown)).foreground_style(theme::ColourRole::secondary_label)),
                         controls::Slider(composition::Binding<double>(std::move(read),
                                              [edit, write = std::move(write)](double value) {
                                                  write(value);
                                                  edit->commit();
                                              }),
                             range, step, components::Text(label))
                             .labels_hidden()),
        id, key);
}

auto SettingRows::action(std::string label, std::string symbol, std::string_view id, std::function<void()> perform) const
    -> composition::Element
{
    const std::string key = rowKey(label, id);
    return described(components::Button(std::move(label), components::Image::SystemName{ std::move(symbol) },
                         [perform = std::move(perform)] { perform(); })
                         .button_style(components::ButtonStyle::glass(theme::Glass::clear())),
        id, key);
}

auto SettingRows::link(std::string label, std::string detail, std::string_view id, std::function<void()> open) const
    -> composition::Element
{
    const std::string key = rowKey(label, id);
    if (!explained(id))
        return structure::DisclosureRow(std::move(label), std::move(detail), [open = std::move(open)] { open(); })
            .id(composition::ElementKey{ key });
    return described(components::Button([open = std::move(open)] { open(); },
                         components::HStack(components::HStackOptions{ .spacing = theme::space::sm },
                             components::Text(std::move(label)), components::Spacer(),
                             components::Text(std::move(detail)).foreground_style(theme::ColourRole::secondary_label),
                             components::Image(components::Image::SystemName{ "chevron_right" })
                                 .foreground_style(theme::ColourRole::tertiary_label)))
                         .button_sizing(components::ButtonSizing::flexible),
        id, key);
}

auto SettingRows::explained(std::string_view id) const -> bool
{
    return !id.empty() && (!help->title(id).empty() || !help->body(id).empty());
}

auto SettingRows::value(std::string label, std::string shown) const -> composition::Element
{
    const std::string key = label;
    return components::LabeledContent(std::move(label), std::move(shown)).id(composition::ElementKey{ key });
}

auto SettingRows::note(std::string shown, bool warning) const -> composition::Element
{
    return components::Text(std::move(shown))
        .font(theme::Font::footnote)
        .foreground_style(warning ? theme::ColourValue(theme::ColourRole::orange)
                                  : theme::ColourValue(theme::ColourRole::secondary_label));
}

auto SettingRows::section(std::string header, std::vector<composition::Element> rows) const -> composition::Element
{
    if (header.empty())
        return structure::Section(structure::Section::Labels{}, std::move(rows));
    return structure::Section(structure::Section::Labels{ .header = components::Text(std::move(header)) }, std::move(rows));
}

}
