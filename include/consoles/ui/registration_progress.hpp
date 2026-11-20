#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "consoles/ui/console_commands.hpp"
#include "consoles/ui/console_shelf.hpp"

namespace akira::consoles::ui {

class RegistrationProgress final : public ::arisa::composition::Component {
public:
    RegistrationProgress(ConsoleShelf::Registering progress, ConsoleCommands& commands, const app::Strings& strings);

    bool operator==(const RegistrationProgress& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    ConsoleShelf::Registering progress;
    ConsoleCommands* commands;
    const app::Strings* strings;
};

}
