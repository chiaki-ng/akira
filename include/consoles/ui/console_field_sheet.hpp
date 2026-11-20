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

class ConsoleFieldSheet final : public ::arisa::composition::Component {
public:
    enum class Field { ConsolePin, Address };

    static const ::arisa::composition::StateKey<std::string> text;

    ConsoleFieldSheet(Field field, std::string catalogName, std::string initial, ConsoleShelf& shelf,
        ConsoleCommands& commands, const app::Strings& strings);

    bool operator==(const ConsoleFieldSheet& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto title() const -> std::string;

    Field field;
    std::string catalogName;
    std::string initial;
    ConsoleShelf* shelf;
    ConsoleCommands* commands;
    const app::Strings* strings;
};

}
