#pragma once

#include <optional>
#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/state_handle.hpp>

#include "app/arisa/art_session.hpp"
#include "app/arisa/notices.hpp"
#include "app/arisa/rail.hpp"
#include "app/arisa/shell_model.hpp"
#include "app/rail_edge.hpp"
#include "app/strings.hpp"
#include "consoles/ui/console_commands.hpp"
#include "consoles/ui/console_shelf.hpp"
#include "play/ui/connecting_screen.hpp"
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
    static constexpr float pageTopUnderBar = Rail::barHeight + (Rail::inset * 2.0F) + 4.0F;
    static constexpr float pageBottomOverBar = Rail::barHeight + (Rail::inset * 2.0F) + 4.0F;
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
    static constexpr ::arisa::composition::Namespace dnsFocus{ "shell-dns" };
    static constexpr float noticeLogoHeight = 70.0F;
    static constexpr float noticeMascotScale = 1.35F;
    static constexpr float noticeWidth = 520.0F;

    ShellScreen(ShellModel& model, Notices& notices, const Parts& parts, const app::Strings& strings);

    bool operator==(const ShellScreen& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    static const ::arisa::composition::StateKey<std::string> linkShown;

    auto linking(::arisa::composition::Element content, ::arisa::composition::Context& context) const
        -> ::arisa::composition::Element;
    auto home(const ShellModel::View& view, RailEdge edge, bool sheet) const -> ::arisa::composition::Element;
    auto loadingArt(::arisa::composition::Context& context, const std::string& console) const
        -> std::vector<play::ui::ConnectingScreen::Art>;
    auto railEdge(::arisa::composition::Context& context) const -> RailEdge;
    auto page(const ShellModel::View& view, RailEdge edge) const -> ::arisa::composition::Element;
    auto pageTitle(ShellModel::Page shown) const -> std::string;
    auto pageContent(ShellModel::Page shown) const -> ::arisa::composition::Element;
    auto settingsPanel(RailEdge edge) const -> ::arisa::composition::Element;
    auto announced(::arisa::composition::Element content, bool streaming) const -> ::arisa::composition::Element;
    auto ending(::arisa::composition::Element content, const ShellModel::View& view,
        ::arisa::composition::Namespace::ID focus) const -> ::arisa::composition::Element;
    auto exitCard(::arisa::composition::Element content, const ShellModel::View& view) const
        -> ::arisa::composition::Element;
    auto endingCard(const ShellModel::Ending& shown, ::arisa::composition::Namespace::ID focus) const
        -> ::arisa::composition::Element;
    auto dnsCard(::arisa::composition::Element content, const ShellModel::View& view,
        ::arisa::composition::Namespace::ID focus) const -> ::arisa::composition::Element;
    auto appletNotice() const -> ::arisa::composition::Element;
    static auto backdrop() -> ::arisa::composition::Element;
    static auto hints(RailEdge edge) -> ::arisa::composition::Element;
    static auto railBand(RailEdge edge) -> float;
    static auto versionText() -> std::string;

    ShellModel* model;
    Notices* notices;
    const Parts* parts;
    const app::Strings* strings;
};

}
