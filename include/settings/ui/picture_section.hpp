#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>

#include "app/strings.hpp"
#include "play/picture_profile.hpp"
#include "settings/help_catalog.hpp"
#include "settings/ui/picture_options.hpp"
#include "settings/ui/setting_rows.hpp"
#include "settings/ui/settings_model.hpp"

namespace akira::settings::ui {

class PictureSection final : public ::arisa::composition::Component {
public:
    static constexpr ::arisa::composition::StateKey<play::StreamProfile> route{ "picture-route",
        play::StreamProfile::Local };

    PictureSection(PictureOptions& options, SettingsModel& model, const HelpCatalog& help, const app::Strings& strings);

    bool operator==(const PictureSection& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto routePicker(::arisa::composition::StateHandle<play::StreamProfile> chosen, bool legacy) const
        -> ::arisa::composition::Element;
    auto cloud(const SettingRows& rows, bool pscloud) const -> std::vector<::arisa::composition::Element>;
    auto adjustments(const SettingRows& rows) const -> ::arisa::composition::Element;
    auto renderer(const SettingRows& rows) const -> std::vector<::arisa::composition::Element>;
    auto stream(const play::PictureProfile& profile) const -> ::arisa::composition::Element;
    auto upscaling(const play::PictureProfile& profile, const PictureOptions::Values& values) const
        -> ::arisa::composition::Element;
    static auto helpId(play::StreamProfile profile, std::string_view row) -> std::string;
    auto upscaleLabel(int target) const -> std::string;

    PictureOptions* options;
    SettingsModel* model;
    const HelpCatalog* help;
    const app::Strings* strings;
};

}
