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

class AddConsoleSheet final : public ::arisa::composition::Component {
public:
    static const ::arisa::composition::StateKey<std::string> name;
    static const ::arisa::composition::StateKey<std::string> address;
    static constexpr ::arisa::composition::StateKey<bool> ps5{ "add-console-ps5", true };

    AddConsoleSheet(ConsoleShelf& shelf, ConsoleCommands& commands, const app::Strings& strings);

    bool operator==(const AddConsoleSheet& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    ConsoleShelf* shelf;
    ConsoleCommands* commands;
    const app::Strings* strings;
};

}
