#include "consoles/ui/recent_rail.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <span>
#include <utility>
#include <vector>

#include "app/arisa/artwork.hpp"

namespace akira::consoles::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

RecentRail::RecentRail(RecentShelf& shelf, ConsoleCommands& commands, const app::Strings& strings, bool continuing)
    : shelf(&shelf)
    , commands(&commands)
    , strings(&strings)
    , continuing(continuing)
{
}

auto RecentRail::body(composition::Context& context) const -> composition::Element
{
    const RecentShelf::View& view = context.read(shelf->view());
    const std::span<const RecentPlay::Title> shown =
        continuing && !view.titles.empty() ? std::span(view.titles).subspan(1) : std::span(view.titles);
    if (shown.empty())
        return components::Group();
    const RecentPlay::Clock::time_point now = RecentPlay::Clock::now();
    const auto focus = context.state(tileFocus);
    std::vector<composition::Element> tiles;
    for (const RecentPlay::Title& title : shown)
        tiles.push_back(tile(view.catalogName, title, now, focus));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::space::md },
        components::Text(strings->format("akira/hosts/recent_title", view.console))
            .font(theme::Font::headline)
            .line_limit(1),
        components::ScrollView(composition::Axes::horizontal, false,
            components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::top,
                                   .spacing = theme::Space{ tileSpacing } },
                std::move(tiles))
                .padding(theme::Space{ edgeInset })))
        .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading })
        .focus_section()
        .id(composition::ElementKey{ "consoles.recent" });
}

auto RecentRail::tile(const std::string& catalogName, const RecentPlay::Title& title,
    RecentPlay::Clock::time_point now, const composition::FocusBinding<std::string>& focus) const
    -> composition::Element
{
    const bool focused = focus.get() == std::optional<std::string>{ title.titleId };
    composition::Element art =
        composition::Element(app::arisa::Artwork(title.coverUrl, "sports_esports"))
            .frame({ .width = artSize, .height = artSize })
            .clip_shape(theme::Shape::rounded(artRadius))
            .overlay(composition::Alignment::center,
                components::Image(components::Image::SystemName{ "play_arrow" })
                    .font(theme::Font::headline)
                    .frame({ .width = playSize, .height = playSize })
                    .glass_effect(theme::Glass::clear(), theme::Shape::circle())
                    .padding(theme::Space{ playInset })
                    .opacity(focused ? 1.0F : 0.0F))
            .overlay(components::ShapeView(theme::Shape::rounded(artRadius))
                         .stroke_border(theme::ColourValue(theme::ColourRole::accent).opacity(focused ? 1.0F : 0.0F),
                             focusBorder))
            .scale_effect(focused ? focusLift : 1.0F);
    composition::Element label =
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                               .spacing = theme::Space{ 3.0F } },
            std::move(art),
            components::Text(title.name)
                .font(theme::Font::subheadline)
                .font_weight(theme::Font::Weight::semibold)
                .line_limit(1)
                .padding(theme::Edge::top, theme::Space{ 6.0F }),
            components::HStack(components::HStackOptions{ .spacing = theme::Space{ 4.0F } },
                components::Image(components::Image::SystemName{ "history" }),
                components::Text(when(title, now)).line_limit(1))
                .font(theme::Font::caption)
                .foreground_style(theme::ColourRole::secondary_label))
            .frame({ .width = artSize, .alignment = composition::Alignment::leading });
    ConsoleCommands* held = commands;
    return components::Button([held, catalogName, titleId = title.titleId] { held->launchTitle(catalogName, titleId); },
        std::move(label))
        .button_style(components::ButtonStyle::plain())
        .focus_effect_disabled()
        .focused(focus, title.titleId)
        .id(composition::ElementKey{ "consoles.recent." + title.titleId });
}

auto RecentRail::when(const RecentPlay::Title& title, RecentPlay::Clock::time_point now) const -> std::string
{
    if (!title.lastUsed)
        return strings->text(title.ps5 ? "akira/hosts/eyebrow_ps5" : "akira/hosts/eyebrow_ps4");
    const RecentPlay::Age age = RecentPlay::age(*title.lastUsed, now);
    return strings->counted(RecentPlay::ageKey(age.span), age.count);
}

}
