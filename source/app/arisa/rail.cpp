#include "app/arisa/rail.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/glass_panel.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/accessibility.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>
#include <vector>

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
    ShellModel* held = model;
    const auto on = [&view](ShellModel::Page page) { return !view.settings && view.page == page; };
    std::vector<composition::Element> stops;
    stops.push_back(stop(strings->text("akira/tabs/consoles"), "sports_esports", on(ShellModel::Page::Consoles),
        [held] { held->show(ShellModel::Page::Consoles); }, "rail.consoles"));
    if (view.cloudOffered)
        stops.push_back(stop(strings->text("akira/cloud/card_title"), "cloud", on(ShellModel::Page::Cloud),
            [held] { held->show(ShellModel::Page::Cloud); }, "rail.cloud"));
    if (view.trophiesOffered)
        stops.push_back(stop(strings->text("akira/tabs/trophies"), "trophy", on(ShellModel::Page::Trophies),
            [held] { held->show(ShellModel::Page::Trophies); }, "rail.trophies"));
    stops.push_back(components::Spacer());
    stops.push_back(stop(strings->text("akira/tabs/settings"), "settings", view.settings,
        [held] { held->showSettings(true); }, "rail.settings"));
    return components::VStack(components::VStackOptions{ .spacing = theme::Space{ spacing } }, std::move(stops))
        .padding(theme::Space{ padding })
        .frame({ .width = width, .max_height = composition::infinity })
        .glass_effect(context.environment(components::Environment::theme).glass().panel,
            components::GlassPanel::shape(components::PanelRadius::large))
        .focus_section()
        .id(composition::ElementKey{ "rail" });
}

auto Rail::stop(std::string title, std::string symbol, bool current, std::function<void()> chosen, std::string key)
    -> composition::Element
{
    const theme::ColourValue tint = current ? theme::ColourValue(theme::ColourRole::accent)
                                            : theme::ColourValue(theme::ColourRole::secondary_label);
    composition::Element label =
        components::VStack(components::VStackOptions{ .spacing = theme::Space{ 3.0F } },
            components::Image(components::Image::SystemName{ std::move(symbol) })
                .font(theme::Font::system(iconSize))
                .foreground_style(tint),
            components::Text(std::move(title))
                .font(theme::Font::caption2)
                .foreground_style(current ? theme::ColourValue(theme::ColourRole::label)
                                          : theme::ColourValue(theme::ColourRole::secondary_label))
                .line_limit(1))
            .padding(theme::Insets::symmetric(theme::Space{ 2.0F }, theme::Space{ 6.0F }))
            .frame({ .max_width = composition::infinity });
    if (current)
        label = std::move(label).background(theme::ColourValue(theme::ColourRole::accent).opacity(currentOpacity),
            theme::Shape::rounded(stopRadius));
    composition::Element button = components::Button(std::move(chosen), std::move(label))
                                      .button_style(components::ButtonStyle::plain())
                                      .id(composition::ElementKey{ std::move(key) });
    if (current)
        button = std::move(button).accessibility_add_traits(composition::AccessibilityTrait::is_selected);
    return button;
}

}
