#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "settings/help_catalog.hpp"
#include "settings/ui/settings_model.hpp"

namespace akira::network::ui {

class NetworkSection final : public ::arisa::composition::Component {
public:
    static constexpr bool is_section = true;

    NetworkSection(settings::ui::SettingsModel& model, const settings::HelpCatalog& help, const app::Strings& strings);

    bool operator==(const NetworkSection& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    settings::ui::SettingsModel* model;
    const settings::HelpCatalog* help;
    const app::Strings* strings;
};

}
