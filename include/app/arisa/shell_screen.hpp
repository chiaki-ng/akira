#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/arisa/shell_commands.hpp"
#include "app/arisa/shell_model.hpp"
#include "app/strings.hpp"

namespace akira::app::arisa {

class ShellScreen final : public ::arisa::composition::Component {
public:
    ShellScreen(const ShellModel& model, ShellCommands& commands, const app::Strings& strings);

    bool operator==(const ShellScreen& other) const;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto consoleList(const ShellModel::View& view) const -> ::arisa::composition::Element;
    auto connecting(const ShellModel::View& view) const -> ::arisa::composition::Element;
    auto streaming() const -> ::arisa::composition::Element;

    const ShellModel* model;
    ShellCommands* commands;
    const app::Strings* strings;
};

}
