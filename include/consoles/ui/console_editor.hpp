#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>

#include "app/strings.hpp"
#include "consoles/ui/console_commands.hpp"
#include "consoles/ui/console_shelf.hpp"

namespace akira::consoles::ui {

class ConsoleEditor final : public ::arisa::composition::Component {
public:
    static const ::arisa::composition::StateKey<ConsoleShelf::Edit> draft;
    static constexpr float width = 520.0F;
    static constexpr float titleSize = 22.0F;

    ConsoleEditor(ConsoleShelf::Editing editing, ConsoleShelf& shelf, ConsoleCommands& commands, const app::Strings& strings);

    bool operator==(const ConsoleEditor& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto capability(const char* key, const char* symbol, bool available) const -> ::arisa::composition::Element;

    ConsoleShelf::Editing editing;
    ConsoleShelf* shelf;
    ConsoleCommands* commands;
    const app::Strings* strings;
};

}
