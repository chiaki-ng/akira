#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/theme/colour_value.hpp>

#include "app/strings.hpp"
#include "consoles/ui/console_commands.hpp"
#include "consoles/ui/console_shelf.hpp"

namespace akira::consoles::ui {

class ConsoleCard final : public ::arisa::composition::Component {
public:
    static constexpr float artWidth = 90.0F;
    static constexpr float artHeight = 120.0F;
    static constexpr float cornerRadius = 24.0F;

    ConsoleCard(ConsoleShelf::Card card, ConsoleCommands& commands, const app::Strings& strings);

    bool operator==(const ConsoleCard& other) const;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto header() const -> ::arisa::composition::Element;
    auto badge() const -> ::arisa::composition::Element;
    auto tag() const -> ::arisa::composition::Element;
    auto actions() const -> ::arisa::composition::Element;
    auto optionsMenu() const -> ::arisa::composition::Element;
    auto statusText() const -> std::string;
    auto statusColour() const -> ::arisa::theme::ColourValue;
    auto actionText() const -> std::string;
    auto optionText(ConsoleEntry::Option option) const -> std::string;
    auto offersWake() const -> bool;

    ConsoleShelf::Card card;
    ConsoleCommands* commands;
    const app::Strings* strings;
};

}
