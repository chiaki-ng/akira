#pragma once

#include <array>
#include <cstddef>
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
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/passed_input.hpp>

#include "app/strings.hpp"
#include "play/stream_controls.hpp"
#include "play/diagnostics_appearance.hpp"
#include "play/diagnostics_report.hpp"
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
    static constexpr float mappingHeight = 520.0F;
    static constexpr ::arisa::provider::Buttons passedButtons{ ::arisa::provider::Button::a, ::arisa::provider::Button::b,
        ::arisa::provider::Button::x, ::arisa::provider::Button::y, ::arisa::provider::Button::dpad_up,
        ::arisa::provider::Button::dpad_down, ::arisa::provider::Button::dpad_left, ::arisa::provider::Button::dpad_right };
    static constexpr ::arisa::runtime::PassedInput passedInput{ .left_stick = true, .right_stick = true, .touch = true };

    struct Session {
        std::string console;
        std::vector<StreamControls::Control> controls;
        bool sleepOffered = false;
        DiagnosticsAppearance diagnostics;
        bool players = false;
        std::optional<std::string> datacenter;
        std::optional<std::string> bandwidth;
        std::optional<StreamControls::Control> opening;

        friend bool operator==(const Session&, const Session&) = default;
    };

    static constexpr ::arisa::composition::StateKey<int> selected{ "stream-dock-selected", 0 };
    static constexpr ::arisa::composition::StateKey<bool> mappingShown{ "stream-dock-mapping", false };
    static const ::arisa::composition::StateKey<DiagnosticsReport> sampled;
    static const ::arisa::composition::StateKey<DiagnosticsAppearance> overlay;
    static constexpr ::arisa::composition::StateKey<int> topic{ "stream-dock-topic", 0 };
    static constexpr float tileMinimum = 138.0F;
    static constexpr float tileSpacing = 12.0F;
    static constexpr float diagnosticsHeight = 470.0F;
    static constexpr std::array<const char*, 3> topics{ "akira/diagnostics/topic_wifi", "akira/diagnostics/topic_speed",
        "akira/diagnostics/topic_resolution" };

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
    auto counter(const DiagnosticsReport::Counter& shown) const -> ::arisa::composition::Element;
    auto panel(StreamControls::Control shown, const DiagnosticsReport& report,
        ::arisa::composition::StateHandle<DiagnosticsAppearance> look, ::arisa::composition::StateHandle<int> chosenTopic,
        const settings::ui::ControlOptions::Values& pad, ::arisa::composition::StateHandle<bool> board) const
        -> ::arisa::composition::Element;
    auto powerPanel() const -> ::arisa::composition::Element;
    auto statsPanel(const DiagnosticsReport& report, ::arisa::composition::StateHandle<DiagnosticsAppearance> look,
        ::arisa::composition::StateHandle<int> chosenTopic) const -> ::arisa::composition::Element;
    template <typename Value, std::size_t Count>
    auto choice(const std::array<Value, Count>& options, auto&& title, ::arisa::composition::StateHandle<DiagnosticsAppearance> look,
        Value DiagnosticsAppearance::* field, const char* key) const -> ::arisa::composition::Element;
    auto troubleshoot(::arisa::composition::StateHandle<int> chosenTopic) const -> ::arisa::composition::Element;
    static auto levelKey(DiagnosticsAppearance::Level level) -> const char*;
    static auto glassKey(DiagnosticsAppearance::Glass glass) -> const char*;
    static auto layoutKey(DiagnosticsAppearance::Layout layout) -> const char*;
    static auto tipCount(int chosen) -> int;
    auto title(StreamControls::Control control) const -> std::string;
    static auto symbol(StreamControls::Control control) -> std::string;
    static auto refresh(::arisa::composition::StateHandle<DiagnosticsReport> report, StreamCommands* commands)
        -> ::arisa::async::Task<void>;

    Session session;
    StreamCommands* commands;
    settings::ui::ControlOptions* controlOptions;
    Mapping* mapping;
    Couch* couch;
    const app::Strings* strings;
};

}
