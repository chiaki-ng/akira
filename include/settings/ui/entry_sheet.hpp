#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>

#include "app/strings.hpp"
#include "settings/ui/settings_model.hpp"

namespace akira::settings::ui {

class EntrySheet final : public ::arisa::composition::Component {
public:
    static const ::arisa::composition::StateKey<std::string> text;

    EntrySheet(SettingsModel::Entry entry, SettingsModel& model, const app::Strings& strings);

    bool operator==(const EntrySheet& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto titleKey(SettingsModel::Entry entry) -> std::string;
    static auto rejectionKey(SettingsModel::Entry entry) -> std::string;

private:
    static auto placeholderKey(SettingsModel::Entry entry) -> std::string;

    SettingsModel::Entry entry;
    SettingsModel* model;
    const app::Strings* strings;
};

}
