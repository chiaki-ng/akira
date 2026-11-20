#pragma once

#include <array>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>

#include "app/strings.hpp"
#include "network/ui/nat_check.hpp"
#include "settings/help_catalog.hpp"
#include "settings/ui/control_options.hpp"
#include "settings/ui/picture_options.hpp"

namespace akira::settings::ui {

class SettingsScreen final : public ::arisa::composition::Component {
public:
    enum class Page { Picture, Controls, Network };

    static constexpr float contentWidth = 760.0F;
    static constexpr std::array<Page, 3> pages{ Page::Picture, Page::Controls, Page::Network };

    static constexpr ::arisa::composition::StateKey<Page> page{ "settings-page", Page::Picture };

    SettingsScreen(PictureOptions& picture, ControlOptions& controlOptions, network::ui::NatCheck& nat,
        const HelpCatalog& help, const app::Strings& strings);

    bool operator==(const SettingsScreen& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto next(Page current, int step) -> Page;

private:
    auto content(Page shown) const -> ::arisa::composition::Element;

    PictureOptions* picture;
    ControlOptions* controlOptions;
    network::ui::NatCheck* nat;
    const HelpCatalog* help;
    const app::Strings* strings;
};

}
