#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "settings/help_catalog.hpp"
#include "settings/takion_target.hpp"
#include "settings/ui/settings_model.hpp"

namespace akira::settings::ui {

class DeveloperSection final : public ::arisa::composition::Component {
public:
    DeveloperSection(SettingsModel& model, const HelpCatalog& help, const app::Strings& strings);

    bool operator==(const DeveloperSection& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto takion(TakionTarget target, const std::string& title, const std::vector<std::uint32_t>& implemented) const
        -> ::arisa::composition::Element;

    SettingsModel* model;
    const HelpCatalog* help;
    const app::Strings* strings;
};

}
