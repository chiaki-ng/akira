#pragma once

#include <optional>
#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>

#include "app/strings.hpp"
#include "consoles/console_pin.hpp"
#include "play/ui/stream_commands.hpp"

namespace akira::play::ui {

class LoginPinCard final : public ::arisa::composition::Component {
public:
    static constexpr float width = 440.0F;
    static constexpr float symbolSide = 52.0F;
    static constexpr float symbolSize = 24.0F;
    static const ::arisa::composition::StateKey<std::string> entered;

    LoginPinCard(bool retry, StreamCommands& commands, const app::Strings& strings);

    bool operator==(const LoginPinCard& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto pin(const std::string& typed) -> std::optional<consoles::ConsolePin>;

private:
    bool retry;
    StreamCommands* commands;
    const app::Strings* strings;
};

}
