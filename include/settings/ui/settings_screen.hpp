#pragma once

#include <vector>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>

#include "app/strings.hpp"
#include "network/ui/nat_check.hpp"
#include "network/ui/tunnel_control.hpp"
#include "play/ui/mapping.hpp"
#include "settings/help_catalog.hpp"
#include "settings/ui/picture_options.hpp"
#include "settings/ui/settings_model.hpp"

namespace akira::settings::ui {

class SettingsScreen final : public ::arisa::composition::Component {
public:
    enum class Page { General, Picture, Controls, Updates, Network, Debug, Config, PowerUser, Developer };

    struct Parts {
        PictureOptions* picture = nullptr;
        SettingsModel* model = nullptr;
        play::ui::Mapping* mapping = nullptr;
        network::ui::NatCheck* nat = nullptr;
        network::ui::TunnelControl* tunnel = nullptr;
        const HelpCatalog* help = nullptr;

        friend bool operator==(const Parts&, const Parts&) = default;
    };

    static constexpr float contentWidth = 760.0F;
    static constexpr float boardWidth = 800.0F;

    static constexpr ::arisa::composition::StateKey<Page> page{ "settings-page", Page::General };

    SettingsScreen(Parts parts, const app::Strings& strings);

    bool operator==(const SettingsScreen& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto pages(const SettingsModel& model) -> std::vector<Page>;
    static auto next(const std::vector<Page>& shown, Page current, int step) -> Page;

private:
    auto content(Page shown) const -> ::arisa::composition::Element;
    auto detail(const SettingsModel::Status& status) const -> ::arisa::composition::Element;
    auto presented(::arisa::composition::Element screen, const SettingsModel::Status& status) const
        -> ::arisa::composition::Element;
    auto title(Page shown) const -> std::string;
    static auto symbol(Page shown) -> std::string;

    Parts parts;
    const app::Strings* strings;
};

}
