#pragma once

#include <cstddef>
#include <functional>
#include <vector>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "play/rumble_profile.hpp"
#include "settings/help_catalog.hpp"
#include "settings/ui/setting_rows.hpp"
#include "settings/ui/settings_model.hpp"

namespace akira::settings::ui {

class PadPage final : public ::arisa::composition::Component {
public:
    PadPage(SettingsModel& model, const HelpCatalog& help, const app::Strings& strings);

    bool operator==(const PadPage& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    using Change = std::function<void(play::RumbleProfile&)>;

    auto edit(std::size_t index, const PadBench::Pad& pad) const -> std::function<void(Change)>;
    auto output(const SettingRows& rows, std::size_t index, const PadBench::Pad& pad, const play::RumbleProfile& profile) const
        -> ::arisa::composition::Element;
    auto rumble(const SettingRows& rows, const PadBench::Pad& pad, const std::function<void(Change)>& change) const
        -> std::vector<::arisa::composition::Element>;
    auto shaping(const SettingRows& rows, const PadBench::Pad& pad, const play::RumbleProfile& profile,
        const std::function<void(Change)>& change) const -> std::vector<::arisa::composition::Element>;
    auto lightbar(const SettingRows& rows, const PadBench::Pad& pad, const play::RumbleProfile& profile,
        const std::function<void(Change)>& change) const -> ::arisa::composition::Element;

    SettingsModel* model;
    const HelpCatalog* help;
    const app::Strings* strings;
};

}
