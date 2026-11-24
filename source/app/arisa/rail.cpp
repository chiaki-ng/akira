#include "app/arisa/rail.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/divider.hpp>
#include <arisa/components/glass_panel.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/controls/menu_style.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <utility>

#include "accounts/ui/account_avatar.hpp"
#include "accounts/ui/account_menu.hpp"

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace theme = ::arisa::theme;

Rail::Rail(ShellModel& model, const app::Strings& strings, accounts::ui::AccountModel* accounts, RailEdge edge)
    : model(&model)
    , strings(&strings)
    , accounts(accounts)
    , edge(edge)
{
}

auto Rail::vertical(RailEdge edge) -> bool
{
    return edge == RailEdge::Left || edge == RailEdge::Right;
}

auto Rail::body(composition::Context& context) const -> composition::Element
{
    const ShellModel::View& view = context.read(model->view());
    const auto focus = context.state(stopFocus);
    const bool upright = vertical(edge);
    const Look look{ .expanded = !upright || focus.get().has_value(), .vertical = upright, .focus = focus };
    ShellModel* held = model;
    std::vector<composition::Element> stops;
    std::vector<std::optional<ShellModel::Page>> shownPages;
    std::size_t index = 0;
    std::size_t current = 0;
    for (const ShellModel::Page page : pages(view))
    {
        if (page == view.page)
            current = index;
        stops.push_back(stop(title(page), symbol(page), page == view.page, [held, page] { held->show(page); }, index++, look));
        shownPages.emplace_back(page);
    }
    stops.push_back(components::Divider());
    stops.push_back(stop(strings->text("akira/tabs/settings"), "settings", false, [held] { held->showSettings(true); },
        index++, look));
    if (std::optional<composition::Element> account = profile(context, index, look))
    {
        stops.push_back(std::move(*account));
        ++index;
    }
    stops.push_back(stop(strings->text("akira/app/exit"), "power_settings_new", false, [held] { held->askExit(); }, index, look));
    composition::Element stack = upright
        ? composition::Element(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                                      .spacing = theme::Space{ stopSpacing } },
              std::move(stops)))
        : composition::Element(components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                                                      .spacing = theme::Space{ stopSpacing } },
              std::move(stops)));
    stack = upright ? std::move(stack).frame({ .width = look.expanded ? expandedWidth : collapsedWidth,
                          .alignment = composition::Alignment::leading })
                    : std::move(stack).frame({ .height = barHeight });
    composition::Element panel = std::move(stack)
                                     .padding(theme::Space{ padding })
                                     .glass_panel(components::PanelRadius::large)
                                     .focus_section()
                                     .on_change(focus.get(),
                                         [held, shownPages](const std::optional<std::size_t>&, const std::optional<std::size_t>& now) {
                                             if (now && *now < shownPages.size() && shownPages[*now])
                                                 held->show(*shownPages[*now]);
                                         })
                                     .on_change(view.railCalls, [focus, current] { focus.set(current); })
                                     .id(composition::ElementKey{ "rail" });
    return placed(std::move(panel), upright && look.expanded);
}

auto Rail::placed(composition::Element panel, bool expanded) const -> composition::Element
{
    if (!vertical(edge))
        return std::move(panel)
            .padding(theme::Space{ inset })
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
                .alignment = edge == RailEdge::Top ? composition::Alignment::top : composition::Alignment::bottom });
    const composition::Alignment side = edge == RailEdge::Left ? composition::Alignment::leading : composition::Alignment::trailing;
    return components::ZStack(components::ZStackOptions{ .alignment = side },
        composition::Element(components::Group())
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
            .background(theme::ColourValue(theme::Colour::hex(0x000000)).opacity(expanded ? dimOpacity : 0.0F))
            .allows_hit_testing(false),
        std::move(panel).padding(theme::Space{ inset }))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity, .alignment = side });
}

auto Rail::stop(std::string title, std::string symbol, bool selected, std::function<void()> perform, std::size_t index,
    const Look& look) const -> composition::Element
{
    std::vector<composition::Element> label;
    label.push_back(components::Image(components::Image::SystemName{ std::move(symbol) })
                        .font(theme::Font::system(iconSize, theme::Font::Weight::medium))
                        .foreground_style(selected ? theme::ColourValue(theme::ColourRole::accent)
                                                   : theme::ColourValue(theme::ColourRole::secondary_label))
                        .frame({ .width = iconColumn, .height = iconColumn }));
    if (look.expanded)
        label.push_back(components::Text(std::move(title))
                            .font(theme::Font::callout)
                            .font_weight(theme::Font::Weight::medium)
                            .foreground_style(selected ? theme::ColourValue(theme::ColourRole::label)
                                                       : theme::ColourValue(theme::ColourRole::secondary_label))
                            .line_limit(1)
                            .fixed_size());
    return components::Button([perform = std::move(perform)] { perform(); },
        shaped(components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                                      .spacing = theme::Space{ labelSpacing } },
                   std::move(label)),
            selected, look))
        .button_style(components::ButtonStyle::plain())
        .content_shape(composition::ContentShapeKinds::interaction | composition::ContentShapeKinds::focus_effect,
            theme::Shape::rounded(stopRadius))
        .focused(look.focus, index)
        .id(composition::ElementKey{ "rail.stop." + std::to_string(index) });
}

auto Rail::profile(composition::Context& context, std::size_t index, const Look& look) const
    -> std::optional<composition::Element>
{
    if (!accounts)
        return std::nullopt;
    const accounts::ui::AccountModel::View& shown = context.read(accounts->view());
    if (shown.entries.empty())
        return std::nullopt;
    const accounts::AccountDesk::Summary summary = shown.summary.value_or(accounts::AccountDesk::Summary{});
    std::vector<composition::Element> label;
    label.push_back(composition::Element(accounts::ui::AccountAvatar(summary.name, summary.avatarUrl, avatarSide))
                        .frame({ .width = iconColumn, .height = iconColumn }));
    if (look.expanded)
        label.push_back(components::Text(summary.name.empty() ? strings->text("akira/settings/no_active_profile") : summary.name)
                            .font(theme::Font::callout)
                            .font_weight(theme::Font::Weight::medium)
                            .foreground_style(theme::ColourRole::secondary_label)
                            .line_limit(1)
                            .fixed_size());
    return controls::Menu(accounts::ui::AccountMenu::choices(*accounts, shown, *strings),
        shaped(components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                                      .spacing = theme::Space{ labelSpacing } },
                   std::move(label)),
            false, look))
        .menu_style(controls::MenuStyle::button())
        .button_style(components::ButtonStyle::plain())
        .content_shape(composition::ContentShapeKinds::interaction | composition::ContentShapeKinds::focus_effect,
            theme::Shape::rounded(stopRadius))
        .focused(look.focus, index)
        .id(composition::ElementKey{ "rail.profile" });
}

auto Rail::shaped(composition::Element label, bool selected, const Look& look) const -> composition::Element
{
    const bool aligned = look.expanded;
    composition::Element framed = std::move(label).padding(
        theme::Insets::symmetric(theme::Space{ aligned ? stopInset : 0.0F }, theme::Space{ 0.0F }));
    framed = look.vertical
        ? std::move(framed).frame({ .max_width = composition::infinity, .min_height = stopHeight,
              .alignment = aligned ? composition::Alignment::leading : composition::Alignment::center })
        : std::move(framed).frame({ .min_height = stopHeight });
    return std::move(framed).background(
        theme::ColourValue(theme::ColourRole::accent).opacity(selected ? selectedOpacity : 0.0F),
        theme::Shape::rounded(stopRadius));
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
