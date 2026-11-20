#pragma once

#include <arisa/appearance/appearance_model.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "settings/help_catalog.hpp"
#include "settings/ui/setting_rows.hpp"
#include "settings/ui/settings_model.hpp"

namespace akira::settings::ui {

class GeneralSection final : public ::arisa::composition::Component {
public:
    GeneralSection(SettingsModel& model, const HelpCatalog& help, const app::Strings& strings);

    bool operator==(const GeneralSection& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto appearance(const ::arisa::appearance::AppearanceModel& look, const SettingRows& rows) const
        -> ::arisa::composition::Element;
    auto accent(const ::arisa::appearance::AppearanceModel& look) const -> ::arisa::composition::Element;
    auto display(const ::arisa::appearance::AppearanceModel& look, const SettingRows& rows) const
        -> ::arisa::composition::Element;
    auto accessibility(const ::arisa::appearance::AppearanceModel& look, const SettingRows& rows) const
        -> ::arisa::composition::Element;
    auto language(const SettingRows& rows) const -> ::arisa::composition::Element;
    auto performance(const SettingRows& rows) const -> ::arisa::composition::Element;
    auto networks(const SettingRows& rows) const -> ::arisa::composition::Element;
    auto about(const SettingRows& rows) const -> ::arisa::composition::Element;
    auto themeLabel(const std::string& pack) const -> std::string;

    SettingsModel* model;
    const HelpCatalog* help;
    const app::Strings* strings;
};

}
