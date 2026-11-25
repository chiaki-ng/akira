#pragma once

#include <optional>
#include <string>
#include <vector>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/focus_binding.hpp>
#include <arisa/composition/focus_state.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/theme/colour_value.hpp>

#include "app/strings.hpp"
#include "cloud/game_actions.hpp"
#include "cloud/launch_checks.hpp"
#include "cloud/ui/cloud_model.hpp"

namespace akira::cloud::ui {

class GamePage final : public ::arisa::composition::Component {
public:
    static constexpr float heroHeight = 230.0F;
    static constexpr float heroRadius = 24.0F;
    static constexpr float coverSide = 168.0F;
    static constexpr float coverRadius = 12.0F;
    static constexpr float logoWidth = 360.0F;
    static constexpr float logoHeight = 110.0F;
    static constexpr float barWidth = 220.0F;
    static constexpr float notesWidth = 640.0F;

    struct Layout {
        std::vector<GameActions::Focus> order;
        bool playable = false;

        friend bool operator==(const Layout&, const Layout&) = default;
    };

    static constexpr ::arisa::composition::FocusState<GameActions::Focus> actionFocus{ "cloud.game.focus" };
    static constexpr ::arisa::composition::StateKey<bool> wantsPlay{ "cloud.game.wants_play", true };

    GamePage(CloudModel& model, const app::Strings& strings);

    static auto layoutOf(const GameActions& shown, bool running) -> Layout;
    static auto repair(const GameActions& shown, const Layout& before, const Layout& after,
        std::optional<GameActions::Focus> held, bool playWanted, bool inside) -> std::optional<GameActions::Focus>;

    bool operator==(const GamePage& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto hero(const CloudModel::Detail& detail) const -> ::arisa::composition::Element;
    auto heading(const CloudModel::Detail& detail) const -> ::arisa::composition::Element;
    auto metadata(const CloudModel::Detail& detail) const -> ::arisa::composition::Element;
    auto actionRow(const CloudModel::Detail& detail, const GameActions& shown,
        ::arisa::composition::FocusBinding<GameActions::Focus> focus) const -> ::arisa::composition::Element;
    static auto repairing(::arisa::composition::Element content, const GameActions& shown, bool running,
        ::arisa::composition::FocusBinding<GameActions::Focus> focus,
        ::arisa::composition::StateHandle<bool> playWanted, bool inside)
        -> ::arisa::composition::Element;
    auto checkRow(const LaunchChecks& checks) const -> ::arisa::composition::Element;
    auto notes(const CloudModel::Detail& detail, const GameActions& shown) const -> ::arisa::composition::Element;
    auto band(const GameDetail& facts) const -> ::arisa::composition::Element;
    auto rating(const GameDetail::Rating& rated) const -> ::arisa::composition::Element;
    auto age(const GameDetail::Age& rated) const -> ::arisa::composition::Element;
    auto factList(const GameDetail& facts) const -> ::arisa::composition::Element;
    auto refreshText(const CloudModel::Refreshing& refreshing) const -> std::string;
    auto titleNotice(const CloudModel::Refreshing& refreshing) const -> std::string;
    auto cardText(const CardCheck& check) const -> std::string;
    static auto markSymbol(LaunchChecks::Mark mark) -> std::string;
    static auto markColour(LaunchChecks::Mark mark) -> ::arisa::theme::ColourRole;
    static auto focusKey(GameActions::Focus focus) -> std::string;

    CloudModel* model;
    const app::Strings* strings;
};

}
