#include "app/arisa/confirmation.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

Confirmation::Confirmation(std::string title, std::string message, std::vector<Choice> choices,
    std::function<void()> dismissed)
    : title(std::move(title))
    , message(std::move(message))
    , choices(std::move(choices))
    , dismissed(std::move(dismissed))
{
}

auto Confirmation::withWidth(float shown) && -> Confirmation
{
    shownWidth = shown;
    return std::move(*this);
}

auto Confirmation::focusing(composition::Namespace::ID chosen) && -> Confirmation
{
    scope = chosen;
    return std::move(*this);
}

auto Confirmation::button(const Choice& choice) const -> composition::Element
{
    auto chosen = [action = choice.chosen] {
        if (action)
            action();
    };
    std::optional<components::ButtonRole> role;
    if (choice.emphasis == Emphasis::Destructive)
        role = components::ButtonRole::destructive;
    composition::Element made = choice.symbol
        ? composition::Element(components::Button(choice.label, components::Image::SystemName{ *choice.symbol }, role, chosen))
        : composition::Element(components::Button(choice.label, role, chosen));
    made = std::move(made).button_style(choice.emphasis == Emphasis::Prominent
            ? components::ButtonStyle::glass_prominent()
            : components::ButtonStyle::glass(theme::Glass::clear()));
    if (choice.disabled)
        made = std::move(made).disabled(true);
    if (scope && choice.emphasis == Emphasis::Prominent)
        made = std::move(made).prefers_default_focus(*scope);
    return std::move(made).id(composition::ElementKey{ choice.key });
}

auto Confirmation::card(std::string key) const -> composition::Element
{
    std::vector<composition::Element> buttons;
    buttons.push_back(components::Spacer());
    for (const Choice& choice : choices)
        buttons.push_back(button(choice));
    std::vector<composition::Element> rows;
    rows.push_back(components::Text(title)
                       .font(theme::Font::system(titleSize, theme::Font::Weight::semibold))
                       .id(composition::ElementKey{ key + ".title" }));
    if (!message.empty())
        rows.push_back(components::Text(message)
                           .font(theme::Font::system(messageSize))
                           .foreground_style(theme::ColourRole::secondary_label)
                           .id(composition::ElementKey{ key + ".message" }));
    rows.push_back(components::HStack(components::HStackOptions{ .spacing = theme::Space{ buttonSpacing } }, std::move(buttons))
                       .padding(theme::Edge::top, theme::Space{ 4.0F }));
    composition::Element shown =
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                               .spacing = theme::Space{ spacing } },
            std::move(rows))
            .padding(theme::Insets::symmetric(theme::Space{ horizontalPadding }, theme::Space{ verticalPadding }))
            .frame({ .width = shownWidth, .alignment = composition::Alignment::leading })
            .glass_panel(components::PanelRadius::modal);
    if (scope)
        shown = std::move(shown).focus_scope(*scope);
    if (dismissed)
        shown = std::move(shown).on_exit_command(choices.empty() ? std::string() : choices.front().label,
            [action = dismissed] { action(); });
    return std::move(shown).id(composition::ElementKey{ std::move(key) });
}

auto Confirmation::dimmed(std::string key) const -> composition::Element
{
    return components::VStack(card(key))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .background(theme::ColourValue(theme::Colour::hex(0x000000)).opacity(scrim))
        .id(composition::ElementKey{ key + ".scrim" });
}

}
