#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/theme/glass.hpp>

#include "app/arisa/notices.hpp"
#include "app/arisa/shell_model.hpp"
#include "app/strings.hpp"
#include "consoles/ui/console_commands.hpp"
#include "consoles/ui/console_shelf.hpp"
#include "network/ui/nat_check.hpp"
#include "play/ui/stream_commands.hpp"
#include "settings/help_catalog.hpp"
#include "settings/ui/control_options.hpp"
#include "settings/ui/picture_options.hpp"

namespace akira::app::arisa {

class ShellScreen final : public ::arisa::composition::Component {
public:
    struct Parts {
        consoles::ui::ConsoleShelf* shelf = nullptr;
        consoles::ui::ConsoleCommands* consoles = nullptr;
        play::ui::StreamCommands* stream = nullptr;
        settings::ui::PictureOptions* picture = nullptr;
        settings::ui::ControlOptions* controls = nullptr;
        network::ui::NatCheck* nat = nullptr;
        const settings::HelpCatalog* help = nullptr;
    };

    static constexpr float pageLeading = 112.0F;
    static constexpr float pageTrailing = 24.0F;
    static constexpr float pageTop = 24.0F;
    static constexpr float pageBottom = 58.0F;
    static constexpr float pageSpacing = 14.0F;
    static constexpr float hintsTrailing = 24.0F;
    static constexpr float hintsBottom = 18.0F;
    static constexpr float settingsSide = 56.0F;
    static constexpr float settingsTop = 16.0F;
    static constexpr float settingsBottom = 50.0F;
    static constexpr float settingsScrim = 0.16F;
    static constexpr float glowOpacity = 0.10F;

    ShellScreen(ShellModel& model, Notices& notices, const Parts& parts, const app::Strings& strings);

    bool operator==(const ShellScreen& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto home() const -> ::arisa::composition::Element;
    auto page() const -> ::arisa::composition::Element;
    auto settingsPanel(::arisa::theme::Glass glass) const -> ::arisa::composition::Element;
    auto announced(::arisa::composition::Element content, bool streaming) const -> ::arisa::composition::Element;
    auto ending(::arisa::composition::Element content, const ShellModel::View& view) const
        -> ::arisa::composition::Element;
    static auto backdrop() -> ::arisa::composition::Element;
    static auto hints() -> ::arisa::composition::Element;
    static auto versionText() -> std::string;

    ShellModel* model;
    Notices* notices;
    const Parts* parts;
    const app::Strings* strings;
};

}
