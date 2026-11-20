#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include "app/strings.hpp"
#include "consoles/ui/console_commands.hpp"

namespace akira::consoles::ui {

class PinPad final : public ::arisa::composition::Component {
public:
    static constexpr std::size_t length = 8;
    static const ::arisa::composition::StateKey<std::string> digits;

    PinPad(std::string catalogName, std::string name, bool ps5, ConsoleCommands& commands, const app::Strings& strings);

    bool operator==(const PinPad& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    static auto boxes(const std::string& entered) -> ::arisa::composition::Element;
    static auto keys(::arisa::composition::StateHandle<std::string> entered) -> ::arisa::composition::Element;

    std::string catalogName;
    std::string name;
    bool ps5;
    ConsoleCommands* commands;
    const app::Strings* strings;
};

}
