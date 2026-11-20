#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "network/ui/nat_check.hpp"
#include "settings/help_catalog.hpp"

namespace akira::network::ui {

class NatSection final : public ::arisa::composition::Component {
public:
    NatSection(NatCheck& check, const settings::HelpCatalog& help, const app::Strings& strings);

    bool operator==(const NatSection& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    NatCheck* check;
    const settings::HelpCatalog* help;
    const app::Strings* strings;
};

}
