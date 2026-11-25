#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "settings/help_catalog.hpp"
#include "settings/ui/settings_model.hpp"

namespace akira::settings::ui {

class ControlsSection final : public ::arisa::composition::Component {
public:
    ControlsSection(SettingsModel& model, const HelpCatalog& help, const app::Strings& strings);

    bool operator==(const ControlsSection& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto summaryKey(PadBench::Summary summary) -> std::string;

private:
    SettingsModel* model;
    const HelpCatalog* help;
    const app::Strings* strings;
};

}
