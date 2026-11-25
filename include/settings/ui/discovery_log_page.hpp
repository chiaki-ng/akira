#pragma once

#include <arisa/async/task.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include <chrono>

#include "app/strings.hpp"
#include "settings/ui/settings_model.hpp"

namespace akira::settings::ui {

class DiscoveryLogPage final : public ::arisa::composition::Component {
public:
    static constexpr std::chrono::milliseconds refreshEvery{ 300 };

    DiscoveryLogPage(SettingsModel& model, const app::Strings& strings);

    bool operator==(const DiscoveryLogPage& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    static auto follow(SettingsModel* model) -> ::arisa::async::Task<void>;

    SettingsModel* model;
    const app::Strings* strings;
};

}
