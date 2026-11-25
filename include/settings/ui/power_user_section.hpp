#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "settings/help_catalog.hpp"
#include "settings/ui/settings_model.hpp"

namespace akira::settings::ui {

class PowerUserSection final : public ::arisa::composition::Component {
public:
    PowerUserSection(SettingsModel& model, const HelpCatalog& help, const app::Strings& strings);

    bool operator==(const PowerUserSection& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    SettingsModel* model;
    const HelpCatalog* help;
    const app::Strings* strings;
};

}
