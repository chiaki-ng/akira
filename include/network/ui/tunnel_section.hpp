#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "network/ui/tunnel_control.hpp"

namespace akira::network::ui {

class TunnelSection final : public ::arisa::composition::Component {
public:
    static constexpr bool is_section = true;

    TunnelSection(TunnelControl& control, const app::Strings& strings);

    bool operator==(const TunnelSection& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    TunnelControl* control;
    const app::Strings* strings;
};

}
