#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "settings/ui/settings_model.hpp"

namespace akira::settings::ui {

class BenchmarkPage final : public ::arisa::composition::Component {
public:
    BenchmarkPage(SettingsModel& model, const app::Strings& strings);

    bool operator==(const BenchmarkPage& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    SettingsModel* model;
    const app::Strings* strings;
};

}
