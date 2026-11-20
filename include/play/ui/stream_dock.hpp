#pragma once

#include <optional>
#include <string>
#include <vector>

#include <arisa/async/task.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include "app/strings.hpp"
#include "play/stream_controls.hpp"
#include "play/stream_stats.hpp"
#include "play/ui/couch.hpp"
#include "play/ui/mapping.hpp"
#include "play/ui/stream_commands.hpp"
#include "settings/ui/control_options.hpp"

namespace akira::play::ui {

class StreamDock final : public ::arisa::composition::Component {
public:
    static constexpr float panelWidth = 700.0F;
    static constexpr float panelPadding = 24.0F;
    static constexpr float panelSpacing = 18.0F;
    static constexpr float dockGap = 14.0F;
    static constexpr float dockBottom = 22.0F;
    static constexpr float tileRadius = 12.0F;

    struct Session {
        std::string console;
        std::vector<StreamControls::Control> controls;
        bool sleepOffered = false;
        StatsOverlayMode statsMode = StatsOverlayMode::Off;
        bool players = false;
        std::optional<StreamControls::Control> opening;

        friend bool operator==(const Session&, const Session&) = default;
    };

    static constexpr ::arisa::composition::StateKey<int> selected{ "stream-dock-selected", 0 };
    static constexpr ::arisa::composition::StateKey<bool> mappingShown{ "stream-dock-mapping", false };
    static const ::arisa::composition::StateKey<StreamStats> sampled;
    static const ::arisa::composition::StateKey<StatsOverlayMode> overlay;

    StreamDock(Session session, StreamCommands& commands, settings::ui::ControlOptions& controlOptions, Mapping& mapping,
        Couch* couch, const app::Strings& strings);

    bool operator==(const StreamDock& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto panels() const -> std::vector<StreamControls::Control>;
    auto opening(const std::vector<StreamControls::Control>& shown) const -> int;
    auto dock(::arisa::composition::StateHandle<int> chosen, const std::vector<StreamControls::Control>& shown) const
        -> ::arisa::composition::Element;
    auto controlsPanel(const settings::ui::ControlOptions::Values& values,
        ::arisa::composition::StateHandle<bool> board) const -> ::arisa::composition::Element;
    auto mappingPanel(::arisa::composition::StateHandle<bool> board) const -> ::arisa::composition::Element;
    auto swipe(settings::ui::ControlOptions::Swipe direction, const char* key, bool enabled) const
        -> ::arisa::composition::Element;
    static auto tile(std::string label, std::string value, std::string unit, ::arisa::theme::ColourValue tone)
        -> ::arisa::composition::Element;
    static auto tone(float value, float good, float fair) -> ::arisa::theme::ColourValue;
    auto panel(StreamControls::Control shown, const StreamStats& stats,
        ::arisa::composition::StateHandle<StatsOverlayMode> mode, const settings::ui::ControlOptions::Values& pad,
        ::arisa::composition::StateHandle<bool> board) const -> ::arisa::composition::Element;
    auto powerPanel() const -> ::arisa::composition::Element;
    auto statsPanel(const StreamStats& stats, ::arisa::composition::StateHandle<StatsOverlayMode> mode) const
        -> ::arisa::composition::Element;
    auto title(StreamControls::Control control) const -> std::string;
    static auto symbol(StreamControls::Control control) -> std::string;
    static auto refresh(::arisa::composition::StateHandle<StreamStats> stats, StreamCommands* commands)
        -> ::arisa::async::Task<void>;

    Session session;
    StreamCommands* commands;
    settings::ui::ControlOptions* controlOptions;
    Mapping* mapping;
    Couch* couch;
    const app::Strings* strings;
};

}
