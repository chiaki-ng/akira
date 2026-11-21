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
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

namespace akira::consoles::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

ConsoleCard::ConsoleCard(ConsoleShelf::Card card, bool compact, ConsoleCommands& commands, const app::Strings& strings)
    : card(std::move(card))
    , compact(compact)
    , commands(&commands)
    , strings(&strings)
{
}

bool ConsoleCard::operator==(const ConsoleCard& other) const
{
    return card == other.card && compact == other.compact && commands == other.commands && strings == other.strings;
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
    return components::HStack(components::HStackOptions{ .spacing = theme::Space{ spacing } }, std::move(art),
        std::move(details))
        .padding(theme::Insets::symmetric(theme::Space{ horizontalPadding },
            theme::Space{ compact ? compactVerticalPadding : verticalPadding }))
        .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading })
        .glass_panel(components::PanelRadius::modal)
        .id(composition::ElementKey{ card.catalogName });
}

auto ConsoleCard::header() const -> composition::Element
{
    return components::HStack(components::HStackOptions{ .spacing = theme::space::sm },
        components::Text(strings->text(card.ps5 ? "akira/hosts/eyebrow_ps5" : "akira/hosts/eyebrow_ps4"))
            .font(theme::Font::system(eyebrowSize, theme::Font::Weight::semibold))
            .tracking(eyebrowTracking)
            .foreground_style(theme::ColourRole::secondary_label),
        components::Spacer(), badge());
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
    return components::HStack(components::HStackOptions{ .spacing = theme::space::sm },
        components::Image(components::Image::SystemName{ symbol }).font(theme::Font::system(14.0F, theme::Font::Weight::medium)),
        components::Text(strings->text(key)).font(theme::Font::system(14.0F)))
        .foreground_style(theme::ColourRole::secondary_label)
        .padding(theme::Insets::symmetric(theme::Space{ 12.0F }, theme::Space{ 7.0F }))
        .background(theme::ColourValue(theme::ColourRole::label).opacity(0.06F), theme::Shape::rounded(tagRadius))
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
            .frame({ .width = 20.0F, .height = 20.0F }))
        .frame({ .width = iconSide, .height = iconSide })
        .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
        .button_border_shape(components::ButtonBorderShape::circle())
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
        auto pressed = [commands = commands, name = card.catalogName] { commands->primary(name); };
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
