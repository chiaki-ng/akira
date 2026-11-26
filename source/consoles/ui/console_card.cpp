#include "consoles/ui/console_card.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_border_shape.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/control_size.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/glass_panel.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/controls/menu_style.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/arisa/artwork.hpp"
#include "app/arisa/hero_figure.hpp"

namespace akira::consoles::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace theme = ::arisa::theme;
namespace graphics = ::arisa::graphics;
namespace motion = ::arisa::motion;

ConsoleCard::ConsoleCard(ConsoleShelf::Card card, bool compact, ConsoleCommands& commands, const app::Strings& strings,
    std::optional<RecentPlay::Title> lastPlayed, std::optional<RecentShelf::Banner> banner)
    : card(std::move(card))
    , compact(compact)
    , commands(&commands)
    , strings(&strings)
    , lastPlayed(std::move(lastPlayed))
    , banner(std::move(banner))
{
}

bool ConsoleCard::operator==(const ConsoleCard& other) const
{
    return card == other.card && compact == other.compact && commands == other.commands && strings == other.strings
        && lastPlayed == other.lastPlayed && banner == other.banner;
}

auto ConsoleCard::body(composition::Context&) const -> composition::Element
{
    composition::Element art = components::Image(card.ps5 ? "img/console/ps5.png" : "img/console/ps4.png")
                                   .resizable()
                                   .aspect_ratio(std::nullopt, composition::ContentMode::fit)
                                   .shadow(theme::ColourValue(theme::Colour::hex(0x000000)).opacity(artShadowOpacity),
                                       artShadowRadius, 0.0F, artShadowDrop)
                                   .frame({ .width = artWidth, .height = artHeight })
                                   .opacity(card.dimmed ? 0.5F : 1.0F);
    composition::Element details = components::VStack(
        components::VStackOptions{ .alignment = components::HorizontalAlignment::leading, .spacing = theme::space::none },
        header(),
        components::Text(card.name)
            .font(theme::Font::system(compact ? compactNameSize : nameSize, theme::Font::Weight::semibold))
            .line_limit(1)
            .padding(theme::Edge::top, theme::Space{ 2.0F }),
        tag().padding(theme::Edge::top, theme::Space{ 8.0F }),
        actions().padding(theme::Edge::top, theme::Space{ 12.0F }))
                                       .frame({ .max_width = composition::infinity,
                                           .alignment = composition::Alignment::leading });
    std::vector<composition::Element> row;
    row.push_back(std::move(art));
    row.push_back(std::move(details));
    if (lastPlayed && !banner && !compact)
        row.push_back(continueButton(*lastPlayed));
    composition::Element content = components::HStack(components::HStackOptions{ .spacing = theme::Space{ spacing } },
        std::move(row))
                                       .padding(theme::Insets::symmetric(theme::Space{ horizontalPadding },
                                           theme::Space{ compact ? compactVerticalPadding : verticalPadding }))
                                       .frame({ .max_width = composition::infinity,
                                           .alignment = composition::Alignment::leading });
    if (banner)
        return layered(std::move(content), *banner).id(composition::ElementKey{ card.catalogName });
    return std::move(content)
        .glass_panel(components::PanelRadius::modal)
        .id(composition::ElementKey{ card.catalogName });
}

auto ConsoleCard::layered(composition::Element content, const RecentShelf::Banner& shown) const -> composition::Element
{
    const auto black = [](float opacity) { return theme::ColourValue(theme::Colour::hex(0x000000)).opacity(opacity); };
    const graphics::Gradient fromBottom(std::vector<graphics::Gradient::Stop>{ { black(bottomShade), 0.0F }, { black(0.0F), 0.52F } });
    const graphics::Gradient fromLeading(std::vector<graphics::Gradient::Stop>{
        { black(leadingShade), 0.0F }, { black(leadingMidShade), 0.38F }, { black(0.0F), 0.62F } });
    std::vector<composition::Element> layers;
    layers.push_back(composition::Element(app::arisa::Artwork(shown.background, ""))
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity }));
    if (!shown.hero.empty())
        layers.push_back(composition::Element(app::arisa::HeroFigure(shown.hero,
            app::arisa::HeroFigure::Placement{ .centre = heroCentre, .roomyWidth = heroRoomyWidth, .spareShift = heroSpareShift,
                .figureHeight = heroFigureHeight, .figureMiddle = heroFigureMiddle }))
                .frame({ .max_width = composition::infinity, .max_height = composition::infinity }));
    layers.push_back(composition::Element(components::Group())
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
            .background(graphics::ShapeStyle(graphics::LinearGradient{ .gradient = fromBottom,
                .start_point = motion::UnitPoint::bottom, .end_point = motion::UnitPoint::top })));
    layers.push_back(composition::Element(components::Group())
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
            .background(graphics::ShapeStyle(graphics::LinearGradient{ .gradient = fromLeading,
                .start_point = motion::UnitPoint::leading, .end_point = motion::UnitPoint::trailing })));
    composition::Element stage = composition::Element(components::ZStack(components::ZStackOptions{ .alignment = composition::Alignment::trailing },
        std::move(layers)))
                                       .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
                                       .allows_hit_testing(false)
                                       .id(composition::ElementKey{ "card.stage" });
    std::vector<composition::Element> heading;
    if (!shown.logo.empty())
        heading.push_back(composition::Element(app::arisa::Artwork(shown.logo, "", app::arisa::Artwork::Scaling::Fit))
                .frame({ .width = compact ? compactLogoWidth : logoWidth, .height = compact ? compactLogoHeight : logoHeight,
                    .alignment = composition::Alignment::trailing })
                .shadow(black(pillShadowOpacity), 10.0F, 0.0F, 4.0F)
                .allows_hit_testing(false));
    heading.push_back(continuePill(shown));
    composition::Element title = composition::Element(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::trailing,
                                                        .spacing = theme::Space{ 16.0F } },
        std::move(heading)))
                                       .padding(theme::Edge::trailing, theme::Space{ titleInset });
    return std::move(content)
        .environment(components::Environment::colour_scheme, std::optional<theme::ColorScheme>(theme::ColorScheme::dark))
        .overlay(composition::Alignment::trailing, std::move(title))
        .background(std::move(stage).clip_shape(theme::Shape::rounded(cardRadius)));
}

auto ConsoleCard::continuePill(const RecentShelf::Banner& shown) const -> composition::Element
{
    std::vector<composition::Element> parts;
    parts.push_back(components::Text(strings->text("akira/hosts/continue")).font_weight(theme::Font::Weight::semibold));
    ConsoleCommands* held = commands;
    return components::Button([held, catalogName = card.catalogName, titleId = shown.titleId] { held->launchTitle(catalogName, titleId); },
        components::HStack(components::HStackOptions{ .spacing = theme::Space{ 8.0F } }, std::move(parts))
            .font(theme::Font::system(13.0F))
            .padding(theme::Insets::symmetric(theme::Space{ 14.0F }, theme::Space{ 7.0F })))
        .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
        .button_border_shape(components::ButtonBorderShape::capsule())
        .environment(components::Environment::colour_scheme, std::optional<theme::ColorScheme>(theme::ColorScheme::dark))
        .id(composition::ElementKey{ card.catalogName + ".continue" });
}

auto ConsoleCard::continueButton(const RecentPlay::Title& title) const -> composition::Element
{
    std::vector<composition::Element> lines;
    lines.push_back(components::Text(strings->text("akira/hosts/continue"))
                        .font(theme::Font::system(13.0F, theme::Font::Weight::semibold))
                        .foreground_style(theme::ColourRole::secondary_label));
    lines.push_back(components::Text(title.name)
                        .font(theme::Font::system(17.0F, theme::Font::Weight::semibold))
                        .line_limit(2));
    if (const std::optional<std::string> when = playedWhen(title))
        lines.push_back(components::Text(*when)
                            .font(theme::Font::system(13.0F))
                            .foreground_style(theme::ColourRole::secondary_label)
                            .line_limit(1));
    ConsoleCommands* held = commands;
    return components::Button([held, catalogName = card.catalogName, titleId = title.titleId] { held->launchTitle(catalogName, titleId); },
        components::HStack(components::HStackOptions{ .spacing = theme::Space{ 16.0F } },
            composition::Element(app::arisa::Artwork(title.coverUrl, "sports_esports"))
                .frame({ .width = continueCover, .height = continueCover })
                .clip_shape(theme::Shape::rounded(continueCoverRadius)),
            components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                   .spacing = theme::Space{ 4.0F } },
                std::move(lines))
                .frame({ .width = continueTextWidth, .alignment = composition::Alignment::leading }))
            .padding(theme::Space{ 10.0F }))
        .button_style(components::ButtonStyle::glass(theme::Glass::regular()))
        .button_border_shape(components::ButtonBorderShape::rounded_rectangle(continueRadius))
        .id(composition::ElementKey{ card.catalogName + ".continue" });
}

auto ConsoleCard::playedWhen(const RecentPlay::Title& title) const -> std::optional<std::string>
{
    if (!title.lastUsed)
        return std::nullopt;
    const RecentPlay::Age age = RecentPlay::age(*title.lastUsed, RecentPlay::Clock::now());
    return strings->counted(RecentPlay::ageKey(age.span), age.count);
}

auto ConsoleCard::header() const -> composition::Element
{
    composition::Element eyebrow = components::Text(strings->text(card.ps5 ? "akira/hosts/eyebrow_ps5" : "akira/hosts/eyebrow_ps4"))
                                       .font(theme::Font::system(eyebrowSize, theme::Font::Weight::semibold))
                                       .tracking(eyebrowTracking)
                                       .foreground_style(theme::ColourRole::secondary_label);
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                                  .spacing = theme::space::md },
        std::move(eyebrow), badge(), components::Spacer());
}

auto ConsoleCard::badge() const -> composition::Element
{
    return components::HStack(components::HStackOptions{ .spacing = theme::Space{ 6.0F } },
        composition::Element(components::Group())
            .frame({ .width = 8.0F, .height = 8.0F })
            .background(statusColour(), theme::Shape::capsule()),
        components::Text(statusText())
            .font(theme::Font::system(12.0F, theme::Font::Weight::semibold))
            .foreground_style(theme::ColourRole::secondary_label)
            .line_limit(1))
        .padding(theme::Insets::symmetric(theme::Space{ 10.0F }, theme::Space{ 5.0F }))
        .background(theme::ColourValue(theme::ColourRole::label).opacity(0.07F), theme::Shape::capsule())
        .id(composition::ElementKey{ card.catalogName + ".status" });
}

auto ConsoleCard::tag() const -> composition::Element
{
    const char* key = "akira/hosts/route_local";
    const char* symbol = "wifi";
    switch (card.badge)
    {
        case ConsoleEntry::Badge::Remote: key = "akira/hosts/route_remote"; symbol = "public"; break;
        case ConsoleEntry::Badge::Manual: key = "akira/hosts/route_manual"; symbol = "link"; break;
        case ConsoleEntry::Badge::Auto:
        case ConsoleEntry::Badge::Local: break;
    }
    const bool selectable = card.routes.size() > 1;
    std::vector<composition::Element> parts;
    parts.push_back(
        components::Image(components::Image::SystemName{ symbol }).font(theme::Font::system(14.0F, theme::Font::Weight::medium)));
    parts.push_back(components::Text(strings->text(key)).font(theme::Font::system(14.0F)));
    if (selectable)
        parts.push_back(components::Image(components::Image::SystemName{ "chevron.down" })
                            .font(theme::Font::system(12.0F, theme::Font::Weight::semibold)));
    composition::Element chip = components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                                                       .spacing = theme::space::sm },
        std::move(parts))
                                    .foreground_style(theme::ColourRole::secondary_label)
                                    .padding(theme::Insets::symmetric(theme::Space{ 12.0F }, theme::Space{ 7.0F }))
                                    .background(theme::ColourValue(theme::ColourRole::label).opacity(0.06F),
                                        theme::Shape::rounded(tagRadius));
    if (!selectable)
        return std::move(chip).id(composition::ElementKey{ card.catalogName + ".route" });
    return controls::Menu(
        [commands = commands, name = card.catalogName, current = card.route, routes = card.routes,
            home = strings->text("akira/hosts/route_local"), internet = strings->text("akira/hosts/route_remote")] {
            std::vector<composition::Element> items;
            for (const ConsoleEntry::Route route : routes)
            {
                std::string label = route == ConsoleEntry::Route::Home ? home : internet;
                if (route == current)
                    label += "  \xe2\x9c\x93";
                items.push_back(
                    components::Button(std::move(label), [commands, name, route] { commands->chooseRoute(name, route); }));
            }
            return composition::Element(components::Group(std::move(items)));
        },
        std::move(chip))
        .menu_style(controls::MenuStyle::button())
        .button_style(components::ButtonStyle::plain())
        .content_shape(composition::ContentShapeKinds::interaction | composition::ContentShapeKinds::focus_effect,
            theme::Shape::rounded(tagRadius))
        .id(composition::ElementKey{ card.catalogName + ".route" });
}

auto ConsoleCard::iconButton(const char* symbol, const char* label, std::string key, bool destructive,
    std::function<void()> pressed) const -> composition::Element
{
    std::optional<components::ButtonRole> role;
    if (destructive)
        role = components::ButtonRole::destructive;
    return components::Button(role, std::move(pressed),
        components::Image(components::Image::SystemName{ symbol })
            .font(theme::Font::system(15.0F, theme::Font::Weight::semibold))
            .foreground_style(destructive ? theme::ColourRole::red : theme::ColourRole::label)
            .frame({ .width = iconSide, .height = iconSide })
            .glass_effect(theme::Glass::clear().interactive(), theme::Shape::circle()))
        .button_style(components::ButtonStyle::plain())
        .content_shape(composition::ContentShapeKinds::interaction | composition::ContentShapeKinds::focus_effect,
            theme::Shape::circle())
        .accessibility_label(strings->text(label))
        .id(composition::ElementKey{ std::move(key) });
}

auto ConsoleCard::actions() const -> composition::Element
{
    std::vector<composition::Element> row;
    if (card.action)
    {
        const bool prominent = *card.action == ConsoleEntry::Action::Register || *card.action == ConsoleEntry::Action::Link;
        const bool plays = *card.action == ConsoleEntry::Action::Connect || *card.action == ConsoleEntry::Action::Wake;
        auto pressed = [commands = commands, name = card.target] { commands->primary(name); };
        composition::Element primary = plays
            ? composition::Element(components::Button(actionText(), components::Image::SystemName{ "play_arrow" }, pressed))
            : composition::Element(components::Button(actionText(), pressed));
        primary = std::move(primary)
                      .font(theme::Font::system(17.0F, theme::Font::Weight::semibold))
                      .control_size(components::ControlSize::large)
                      .id(composition::ElementKey{ card.catalogName + ".primary" });
        primary = prominent ? std::move(primary).button_style(components::ButtonStyle::glass_prominent())
                            : std::move(primary).button_style(components::ButtonStyle::glass(theme::Glass::clear()));
        row.push_back(std::move(primary));
    }
    if (!card.options.empty())
        row.push_back(iconButton("edit", "akira/hosts/edit_console", card.catalogName + ".edit", false,
            [commands = commands, name = card.catalogName] { commands->edit(name); }));
    if (std::ranges::contains(card.options, ConsoleEntry::Option::Delete))
        row.push_back(iconButton("delete", "akira/hosts/delete_console", card.catalogName + ".delete", true,
            [commands = commands, name = card.catalogName] { commands->askDelete(name); }));
    if (missingLocally())
        row.push_back(components::Text(strings->text("akira/hosts/missing_locally"))
                          .font(theme::Font::callout)
                          .foreground_style(theme::ColourRole::secondary_label)
                          .line_limit(1)
                          .padding(theme::Edge::leading, theme::Space{ 4.0F }));
    return components::HStack(components::HStackOptions{ .spacing = theme::space::md }, std::move(row));
}

auto ConsoleCard::missingLocally() const -> bool
{
    return ConsoleShelf::presenceOf(card) == ConsoleShelf::Presence::NotNearby && card.badge != ConsoleEntry::Badge::Remote;
}

auto ConsoleCard::statusText() const -> std::string
{
    switch (card.status)
    {
        case ConsoleEntry::Status::Link: return strings->text("akira/hosts/link");
        case ConsoleEntry::Status::Register: return strings->text("akira/hosts/not_registered");
        case ConsoleEntry::Status::Ready:
        case ConsoleEntry::Status::Standby:
        case ConsoleEntry::Status::Presence: break;
    }
    switch (ConsoleShelf::presenceOf(card))
    {
        case ConsoleShelf::Presence::Ready: return strings->text("akira/hosts/ready");
        case ConsoleShelf::Presence::RestMode: return strings->text("akira/hosts/rest_mode");
        case ConsoleShelf::Presence::NotNearby: return strings->text("akira/hosts/not_nearby");
        case ConsoleShelf::Presence::Unknown: break;
    }
    return card.presence;
}

auto ConsoleCard::statusColour() const -> theme::ColourValue
{
    switch (card.status)
    {
        case ConsoleEntry::Status::Link: return theme::ColourRole::accent;
        case ConsoleEntry::Status::Register: return theme::ColourRole::tertiary_label;
        case ConsoleEntry::Status::Ready:
        case ConsoleEntry::Status::Standby:
        case ConsoleEntry::Status::Presence: break;
    }
    switch (ConsoleShelf::presenceOf(card))
    {
        case ConsoleShelf::Presence::Ready: return theme::Colour{ .red = 0.45F, .green = 0.92F, .blue = 0.18F, .alpha = 1.0F };
        case ConsoleShelf::Presence::RestMode: return theme::Colour{ .red = 0.95F, .green = 0.72F, .blue = 0.29F, .alpha = 1.0F };
        case ConsoleShelf::Presence::NotNearby:
        case ConsoleShelf::Presence::Unknown: break;
    }
    return theme::ColourRole::secondary_label;
}

auto ConsoleCard::actionText() const -> std::string
{
    if (!card.action)
        return {};
    switch (*card.action)
    {
        case ConsoleEntry::Action::Connect: return strings->text("akira/hosts/play");
        case ConsoleEntry::Action::Wake: return strings->text("akira/hosts/wake_and_play");
        case ConsoleEntry::Action::Register: return strings->text("akira/hosts/register");
        case ConsoleEntry::Action::Link: return strings->text("akira/hosts/link");
    }
    return {};
}

}
