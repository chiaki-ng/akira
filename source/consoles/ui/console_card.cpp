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
#include <arisa/controls/menu.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>
#include <vector>

namespace akira::consoles::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
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

auto ConsoleCard::body(composition::Context& context) const -> composition::Element
{
    composition::Element art = components::Image(card.ps5 ? "img/console/ps5.png" : "img/console/ps4.png")
                                   .resizable()
                                   .aspect_ratio(std::nullopt, composition::ContentMode::fit)
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
        .glass_effect(context.environment(components::Environment::theme).glass().panel,
            components::GlassPanel::shape(components::PanelRadius::modal))
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
    const char* symbol = "home";
    switch (card.badge)
    {
        case ConsoleEntry::Badge::Remote: key = "akira/hosts/route_remote"; symbol = "public"; break;
        case ConsoleEntry::Badge::Manual: key = "akira/hosts/route_manual"; symbol = "link"; break;
        case ConsoleEntry::Badge::Auto:
        case ConsoleEntry::Badge::Local: break;
    }
    return components::HStack(components::HStackOptions{ .spacing = theme::space::sm },
        components::Image(components::Image::SystemName{ symbol }).font(theme::Font::system(15.0F)),
        components::Text(strings->text(key)).font(theme::Font::callout))
        .padding(theme::Insets::symmetric(theme::Space{ 12.0F }, theme::Space{ 5.0F }))
        .background(theme::ColourValue(theme::ColourRole::label).opacity(0.06F), theme::Shape::rounded(tagRadius))
        .overlay(components::ShapeView(theme::Shape::rounded(tagRadius))
                     .stroke_border(theme::ColourValue(theme::ColourRole::label).opacity(0.22F), 1.0F))
        .id(composition::ElementKey{ card.catalogName + ".route" });
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
                      .font(theme::Font::headline)
                      .control_size(components::ControlSize::large)
                      .id(composition::ElementKey{ card.catalogName + ".primary" });
        primary = prominent ? std::move(primary).button_style(components::ButtonStyle::glass_prominent())
                            : std::move(primary).button_style(components::ButtonStyle::glass(theme::Glass::clear()));
        row.push_back(std::move(primary));
    }
    row.push_back(optionsMenu());
    return components::HStack(components::HStackOptions{ .spacing = theme::space::md }, std::move(row));
}

auto ConsoleCard::optionsMenu() const -> composition::Element
{
    std::vector<std::string> labels;
    for (const ConsoleEntry::Option option : card.options)
        labels.push_back(optionText(option));
    return controls::Menu(
        [options = card.options, labels = std::move(labels), wake = offersWake(), commands = commands,
            wakeText = strings->text("akira/hosts/wake"), name = card.catalogName] {
            std::vector<composition::Element> items;
            if (wake)
                items.push_back(components::Button(wakeText, [commands, name] { commands->wake(name); }));
            for (std::size_t index = 0; index < options.size(); ++index)
            {
                const ConsoleEntry::Option option = options[index];
                std::optional<components::ButtonRole> role;
                if (option == ConsoleEntry::Option::Delete)
                    role = components::ButtonRole::destructive;
                items.push_back(components::Button(labels[index], role,
                    [commands, name, option] { commands->option(name, option); }));
            }
            return composition::Element(components::Group(std::move(items)));
        },
        components::Image(components::Image::SystemName{ "more_horiz" }).frame({ .width = 24.0F, .height = 24.0F }))
        .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
        .button_border_shape(components::ButtonBorderShape::circle())
        .control_size(components::ControlSize::large)
        .id(composition::ElementKey{ card.catalogName + ".options" });
}

auto ConsoleCard::offersWake() const -> bool
{
    return card.badge != ConsoleEntry::Badge::Remote
        && (card.action == ConsoleEntry::Action::Wake || card.action == ConsoleEntry::Action::Connect);
}

auto ConsoleCard::statusText() const -> std::string
{
    switch (card.status)
    {
        case ConsoleEntry::Status::Link: return strings->text("akira/hosts/link");
        case ConsoleEntry::Status::Ready: return strings->text("akira/hosts/ready");
        case ConsoleEntry::Status::Register: return strings->text("akira/hosts/register");
        case ConsoleEntry::Status::Standby: return strings->text("akira/hosts/standby");
        case ConsoleEntry::Status::Presence: break;
    }
    return card.presence;
}

auto ConsoleCard::statusColour() const -> theme::ColourValue
{
    switch (card.status)
    {
        case ConsoleEntry::Status::Ready: return theme::ColourRole::green;
        case ConsoleEntry::Status::Standby: return theme::ColourRole::orange;
        case ConsoleEntry::Status::Link:
        case ConsoleEntry::Status::Register: return theme::ColourRole::accent;
        case ConsoleEntry::Status::Presence: break;
    }
    return theme::ColourRole::tertiary_label;
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

auto ConsoleCard::optionText(ConsoleEntry::Option option) const -> std::string
{
    switch (option)
    {
        case ConsoleEntry::Option::ConsolePin: return strings->text("akira/host_settings/console_pin");
        case ConsoleEntry::Option::AddAddress: return strings->text("akira/hosts/add_address");
        case ConsoleEntry::Option::SetAddress: return strings->text("akira/hosts/set_address");
        case ConsoleEntry::Option::TakionVersion: return strings->text("akira/hosts/takion_version");
        case ConsoleEntry::Option::Delete: return strings->text("akira/hosts/delete");
    }
    return {};
}

}
