#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/composition/namespace.hpp>

#include "app/arisa/art_session.hpp"
#include "app/arisa/notices.hpp"
#include "app/arisa/shell_model.hpp"
#include "app/strings.hpp"
#include "consoles/ui/console_commands.hpp"
#include "consoles/ui/console_shelf.hpp"
#include "play/ui/stream_commands.hpp"
#include "settings/ui/settings_screen.hpp"

namespace akira::trophies::ui {
class TrophyModel;
}

namespace akira::cloud::ui {
class CloudModel;
}

namespace akira::consoles::ui {
class RecentShelf;
}

namespace akira::app::arisa {

class ShellScreen final : public ::arisa::composition::Component {
public:
    struct Parts {
        consoles::ui::ConsoleShelf* shelf = nullptr;
        consoles::ui::ConsoleCommands* consoles = nullptr;
        play::ui::StreamCommands* stream = nullptr;
        settings::ui::SettingsScreen::Parts settings;
        const ArtSession* art = nullptr;
        consoles::ui::RecentShelf* recent = nullptr;
        trophies::ui::TrophyModel* trophies = nullptr;
        cloud::ui::CloudModel* cloud = nullptr;
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
    static constexpr ::arisa::composition::Namespace endingFocus{ "shell-ending" };

    ShellScreen(ShellModel& model, Notices& notices, const Parts& parts, const app::Strings& strings);

    bool operator==(const ShellScreen& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto home(ShellModel::Page shown) const -> ::arisa::composition::Element;
    auto page(ShellModel::Page shown) const -> ::arisa::composition::Element;
    auto pageTitle(ShellModel::Page shown) const -> std::string;
    auto pageContent(ShellModel::Page shown) const -> ::arisa::composition::Element;
    auto settingsPanel(::arisa::theme::Glass glass) const -> ::arisa::composition::Element;
    auto announced(::arisa::composition::Element content, bool streaming) const -> ::arisa::composition::Element;
    auto ending(::arisa::composition::Element content, const ShellModel::View& view,
        ::arisa::composition::Namespace::ID focus) const -> ::arisa::composition::Element;
    auto exitCard(::arisa::composition::Element content, const ShellModel::View& view) const
        -> ::arisa::composition::Element;
    auto endingCard(const ShellModel::Ending& shown, ::arisa::composition::Namespace::ID focus) const
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
