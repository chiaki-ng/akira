#include "app/arisa/rail.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/divider.hpp>
#include <arisa/components/glass_panel.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

Rail::Rail(ShellModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto Rail::body(composition::Context& context) const -> composition::Element
{
    const ShellModel::View& view = context.read(model->view());
    const auto focus = context.state(stopFocus);
    const auto open = context.state(pinned);
    const bool expanded = open.get() || focus.get().has_value();
    ShellModel* held = model;
    std::vector<composition::Element> stops;
    stops.push_back(stop(Stop{ .title = strings->text("akira/rail/menu"),
                             .symbol = "menu",
                             .selected = false,
                             .perform = [open] { open.set(!open.get()); } },
        menuStop, expanded, focus));
    std::size_t index = menuStop + 1;
    for (const ShellModel::Page page : pages(view))
        stops.push_back(stop(Stop{ .title = title(page),
                                 .symbol = symbol(page),
                                 .selected = page == view.page,
                                 .perform = [held, page] { held->show(page); } },
            index++, expanded, focus));
    stops.push_back(components::Divider());
    stops.push_back(stop(Stop{ .title = strings->text("akira/tabs/settings"),
                             .symbol = "settings",
                             .selected = false,
                             .perform = [held] { held->showSettings(true); } },
        index++, expanded, focus));
    stops.push_back(stop(Stop{ .title = strings->text("akira/app/exit"),
                             .symbol = "power_settings_new",
                             .selected = false,
                             .perform = [held] { held->askExit(); } },
        index, expanded, focus));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ stopSpacing } },
        std::move(stops))
        .padding(theme::Space{ padding })
        .frame({ .width = (expanded ? expandedWidth : collapsedWidth), .alignment = composition::Alignment::leading })
        .glass_panel(components::PanelRadius::large)
        .focus_section()
        .id(composition::ElementKey{ "rail" });
}

auto Rail::stop(Stop shown, std::size_t index, bool expanded,
    composition::FocusBinding<std::optional<std::size_t>> focus) const -> composition::Element
{
    std::vector<composition::Element> label;
    label.push_back(components::Image(components::Image::SystemName{ shown.symbol })
                        .font(theme::Font::system(iconSize, theme::Font::Weight::medium))
                        .foreground_style(shown.selected ? theme::ColourValue(theme::ColourRole::accent)
                                                         : theme::ColourValue(theme::ColourRole::secondary_label))
                        .frame({ .width = iconColumn, .height = iconColumn }));
    if (expanded)
        label.push_back(components::Text(shown.title)
                            .font(theme::Font::callout)
                            .font_weight(theme::Font::Weight::medium)
                            .foreground_style(shown.selected ? theme::ColourValue(theme::ColourRole::label)
                                                             : theme::ColourValue(theme::ColourRole::secondary_label))
                            .line_limit(1));
    const std::string key = "rail.stop." + std::to_string(index);
    return components::Button(std::move(shown.perform),
        components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                               .spacing = theme::Space{ labelSpacing } },
            std::move(label))
            .padding(theme::Insets::symmetric(theme::Space{ expanded ? stopInset : 0.0F }, theme::Space{ 0.0F }))
            .frame({ .max_width = composition::infinity, .min_height = stopHeight,
                .alignment = expanded ? composition::Alignment::leading : composition::Alignment::center })
            .background(theme::ColourValue(theme::ColourRole::accent).opacity(shown.selected ? selectedOpacity : 0.0F),
                theme::Shape::rounded(stopRadius)))
        .button_style(components::ButtonStyle::plain())
        .content_shape(composition::ContentShapeKinds::interaction | composition::ContentShapeKinds::focus_effect,
            theme::Shape::rounded(stopRadius))
        .focused(focus, std::optional<std::size_t>{ index })
        .id(composition::ElementKey{ key });
}

auto Rail::pages(const ShellModel::View& view) -> std::vector<ShellModel::Page>
{
    std::vector<ShellModel::Page> shown{ ShellModel::Page::Consoles };
    if (view.cloudOffered)
        shown.push_back(ShellModel::Page::Cloud);
    if (view.trophiesOffered)
        shown.push_back(ShellModel::Page::Trophies);
    return shown;
}

auto Rail::title(ShellModel::Page page) const -> std::string
{
    switch (page)
    {
        case ShellModel::Page::Consoles: return strings->text("akira/tabs/consoles");
        case ShellModel::Page::Cloud: return strings->text("akira/cloud/card_title");
        case ShellModel::Page::Trophies: return strings->text("akira/tabs/trophies");
    }
    return {};
}

auto Rail::symbol(ShellModel::Page page) -> std::string
{
    switch (page)
    {
        case ShellModel::Page::Consoles: return "sports_esports";
        case ShellModel::Page::Cloud: return "cloud";
        case ShellModel::Page::Trophies: return "trophy";
    }
    return "circle";
}

}
