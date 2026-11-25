#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/theme/colour_value.hpp>

#include "app/strings.hpp"
#include "settings/config_listing.hpp"
#include "settings/ui/settings_model.hpp"

namespace akira::settings::ui {

class ConfigPage final : public ::arisa::composition::Component {
public:
    static constexpr ::arisa::composition::StateKey<bool> revealed{ "config-revealed", false };

    ConfigPage(SettingsModel& model, const app::Strings& strings);

    bool operator==(const ConfigPage& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    static auto tone(ConfigListing::Token token) -> ::arisa::theme::ColourValue;
    static auto line(const ConfigListing::Line& shown) -> ::arisa::composition::Element;

    SettingsModel* model;
    const app::Strings* strings;
};

}
