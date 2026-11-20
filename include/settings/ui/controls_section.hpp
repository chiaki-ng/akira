#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "settings/help_catalog.hpp"
#include "settings/ui/control_options.hpp"

namespace akira::settings::ui {

class ControlsSection final : public ::arisa::composition::Component {
public:
    ControlsSection(ControlOptions& options, const HelpCatalog& help, const app::Strings& strings);

    bool operator==(const ControlsSection& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto touchpad(const ControlOptions::Values& values) const -> ::arisa::composition::Element;
    auto rumble() const -> ::arisa::composition::Element;
    auto swipe(ControlOptions::Swipe direction, const char* key, const char* id) const -> ::arisa::composition::Element;

    ControlOptions* options;
    const HelpCatalog* help;
    const app::Strings* strings;
};

}
