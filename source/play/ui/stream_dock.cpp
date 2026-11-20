#include "play/ui/stream_dock.hpp"

#include <arisa/async/awaitables.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_sizing.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/control_size.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/controls/toggle_style.hpp>
#include <arisa/structure/dock_bar.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <chrono>
#include <format>
#include <utility>
#include <vector>

#include "play/stream_summary.hpp"

namespace akira::play::ui {

namespace async = ::arisa::async;
namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

const composition::StateKey<StreamStats> StreamDock::sampled{ "stream-dock-stats", StreamStats{} };
const composition::StateKey<StatsOverlayMode> StreamDock::overlay{ "stream-dock-overlay", StatsOverlayMode::Off };

StreamDock::StreamDock(Session session, StreamCommands& commands, settings::ui::ControlOptions& controlOptions,
    const app::Strings& strings)
    : session(std::move(session))
    , commands(&commands)
    , controlOptions(&controlOptions)
    , strings(&strings)
{
}

auto StreamDock::panels() const -> std::vector<StreamControls::Control>
{
    std::vector<StreamControls::Control> shown;
    if (std::ranges::contains(session.controls, StreamControls::Control::Stats))
        shown.push_back(StreamControls::Control::Stats);
    if (std::ranges::contains(session.controls, StreamControls::Control::Pad)
        || std::ranges::contains(session.controls, StreamControls::Control::Motion))
        shown.push_back(StreamControls::Control::Pad);
    if (std::ranges::contains(session.controls, StreamControls::Control::Power))
        shown.push_back(StreamControls::Control::Power);
    return shown;
}

auto StreamDock::body(composition::Context& context) const -> composition::Element
{
    const auto chosen = context.state(selected);
    const auto stats = context.state(sampled);
    const auto mode = context.state(overlay, session.statsMode);
    const settings::ui::ControlOptions::Values& pad = context.read(controlOptions->view());
    const std::vector<StreamControls::Control> shown = panels();
    const StreamControls::Control current = shown.empty()
        ? StreamControls::Control::Power
        : shown[static_cast<std::size_t>(std::clamp(chosen.get(), 0, static_cast<int>(shown.size()) - 1))];
    auto resume = [commands = commands] { commands->resume(); };
    return components::VStack(components::VStackOptions{ .spacing = theme::Space{ dockGap } },
        panel(current, stats.get(), mode, pad), dock(chosen, shown))
        .padding(theme::Edge::bottom, theme::Space{ dockBottom })
        .on_exit_command(strings->text("akira/stream_menu/hint_resume"), resume)
        .task([stats, commands = commands] { return refresh(stats, commands); })
        .id(composition::ElementKey{ "stream-dock" });
}

auto StreamDock::refresh(composition::StateHandle<StreamStats> stats, StreamCommands* commands) -> async::Task<void>
{
    while (true)
    {
        stats.set(commands->stats());
        co_await async::sleep(std::chrono::seconds{ 1 });
    }
}

auto StreamDock::dock(composition::StateHandle<int> chosen, const std::vector<StreamControls::Control>& shown) const
    -> composition::Element
{
    std::vector<structure::DockBar::Item> items;
    for (const StreamControls::Control control : shown)
        items.push_back(structure::DockBar::Item{ .title = title(control), .system_image = symbol(control) });
    std::vector<structure::DockBar::Action> trailing;
    trailing.push_back(structure::DockBar::Action{ .title = strings->text("akira/common/close"),
        .system_image = "close",
        .perform = [commands = commands] { commands->resume(); } });
    composition::Binding<std::size_t> selection([chosen] { return static_cast<std::size_t>(std::max(chosen.get(), 0)); },
        [chosen](std::size_t index) { chosen.set(static_cast<int>(index)); });
    return composition::Element(structure::DockBar(std::move(selection), std::move(items), std::move(trailing)))
        .focus_section()
        .id(composition::ElementKey{ "stream-dock.tabs" });
}

auto StreamDock::panel(StreamControls::Control shown, const StreamStats& stats,
    composition::StateHandle<StatsOverlayMode> mode, const settings::ui::ControlOptions::Values& pad) const
    -> composition::Element
{
    composition::Element content = powerPanel();
    if (shown == StreamControls::Control::Stats)
        content = statsPanel(stats, mode);
    else if (shown == StreamControls::Control::Pad)
        content = controlsPanel(pad);
    return std::move(content)
        .padding(theme::Space{ panelPadding })
        .frame({ .width = panelWidth, .alignment = composition::Alignment::leading })
        .glass_panel(components::PanelRadius::large)
        .id(composition::ElementKey{ "stream-dock.panel" });
}

auto StreamDock::powerPanel() const -> composition::Element
{
    std::vector<composition::Element> rows;
    rows.push_back(components::Text(strings->text("akira/stream_menu/panel_power_title"))
                       .font(theme::Font::title2)
                       .font_weight(theme::Font::Weight::semibold));
    rows.push_back(components::Button(strings->text("akira/stream_menu/opt_disconnect"),
        [commands = commands] { commands->leave(false); })
                       .font(theme::Font::headline)
                       .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                       .control_size(components::ControlSize::large)
                       .button_sizing(components::ButtonSizing::flexible)
                       .id(composition::ElementKey{ "stream-dock.disconnect" }));
    if (session.sleepOffered)
        rows.push_back(components::Button(strings->text("akira/stream_menu/opt_disconnect_sleep"),
            components::Image::SystemName{ "power_settings_new" }, components::ButtonRole::destructive,
            [commands = commands] { commands->leave(true); })
                           .font(theme::Font::headline)
                           .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                           .control_size(components::ControlSize::large)
                           .button_sizing(components::ButtonSizing::flexible)
                           .id(composition::ElementKey{ "stream-dock.sleep" }));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ panelSpacing } },
        std::move(rows));
}

auto StreamDock::swipe(settings::ui::ControlOptions::Swipe direction, const char* key, bool enabled) const
    -> composition::Element
{
    settings::ui::ControlOptions* edit = controlOptions;
    return controls::Toggle(strings->text(key),
        composition::Binding<bool>([enabled] { return enabled; },
            [edit, direction](bool value) { edit->setSwipe(direction, value); }));
}

auto StreamDock::controlsPanel(const settings::ui::ControlOptions::Values& values) const -> composition::Element
{
    using Swipe = settings::ui::ControlOptions::Swipe;
    settings::ui::ControlOptions* edit = controlOptions;
    std::vector<composition::Element> rows;
    rows.push_back(components::Text(strings->text("akira/stream_menu/card_pad"))
                       .font(theme::Font::title2)
                       .font_weight(theme::Font::Weight::semibold));
    rows.push_back(controls::Toggle(strings->text("akira/settings/touchpad"),
        composition::Binding<bool>([touchpad = values.touchpad] { return touchpad; },
            [edit](bool value) { edit->setTouchpad(value); }))
                       .id(composition::ElementKey{ "stream-dock.touchpad" }));
    if (values.touchpad)
        rows.push_back(components::HStack(components::HStackOptions{ .spacing = theme::space::lg },
            swipe(Swipe::Up, "akira/settings/swipe_up", values.swipeUp),
            swipe(Swipe::Down, "akira/settings/swipe_down", values.swipeDown),
            swipe(Swipe::Left, "akira/settings/swipe_left", values.swipeLeft),
            swipe(Swipe::Right, "akira/settings/swipe_right", values.swipeRight))
                           .toggle_style(controls::ToggleStyle::checkbox()));
    if (std::ranges::contains(session.controls, StreamControls::Control::Motion))
        rows.push_back(components::Button(title(StreamControls::Control::Motion),
            components::Image::SystemName{ symbol(StreamControls::Control::Motion) },
            [commands = commands] {
                commands->resetMotion();
                commands->resume();
            })
                           .font(theme::Font::headline)
                           .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                           .control_size(components::ControlSize::large)
                           .button_sizing(components::ButtonSizing::flexible)
                           .id(composition::ElementKey{ "stream-dock.motion" }));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ panelSpacing } },
        std::move(rows));
}

auto StreamDock::tone(float value, float good, float fair) -> theme::ColourValue
{
    if (value < good)
        return theme::ColourRole::green;
    if (value < fair)
        return theme::ColourRole::orange;
    return theme::ColourRole::red;
}

auto StreamDock::tile(std::string label, std::string value, std::string unit, theme::ColourValue shade)
    -> composition::Element
{
    composition::Element reading = components::Text(std::move(value))
                                       .font(theme::Font::system(20.0F, theme::Font::Weight::medium))
                                       .foreground_style(shade)
                                       .line_limit(1);
    if (!unit.empty())
        reading = components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::last_text_baseline,
                                         .spacing = theme::Space{ 3.0F } },
            std::move(reading),
            components::Text(std::move(unit)).font(theme::Font::caption).foreground_style(theme::ColourRole::secondary_label));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 5.0F } },
        components::Text(std::move(label)).font(theme::Font::caption).foreground_style(theme::ColourRole::secondary_label),
        std::move(reading))
        .padding(theme::Space{ 10.0F })
        .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading })
        .background(theme::ColourValue(theme::ColourRole::label).opacity(0.07F), theme::Shape::rounded(tileRadius));
}

auto StreamDock::statsPanel(const StreamStats& stats, composition::StateHandle<StatsOverlayMode> mode) const
    -> composition::Element
{
    const StreamSummary summary(stats);
    theme::ColourValue health = theme::ColourRole::green;
    if (summary.health() == StreamSummary::Health::Fair)
        health = theme::ColourRole::orange;
    else if (summary.health() == StreamSummary::Health::Poor)
        health = theme::ColourRole::red;
    const theme::ColourValue plain = theme::ColourRole::label;
    const float fpsShare = stats.requested_fps > 0 ? stats.fps / static_cast<float>(stats.requested_fps) : 1.0F;
    const theme::ColourValue fpsTone = tone(1.0F - fpsShare, 0.08F, 0.25F);
    const std::string missing = "--";
    const std::string off = strings->text("akira/stream_menu/opt_off");
    const std::string compact = strings->text("akira/stream_menu/opt_compact");
    const std::string full = strings->text("akira/stream_menu/opt_full");
    StreamCommands* sink = commands;
    composition::Binding<StatsOverlayMode> picked(
        [mode] { return mode.get(); },
        [mode, sink](StatsOverlayMode value) {
            mode.set(value);
            sink->showStats(value);
        });
    auto text = [this](const char* key) { return strings->text(key); };
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 14.0F } },
        components::Text(text("akira/stream_menu/card_stats"))
            .font(theme::Font::title2)
            .font_weight(theme::Font::Weight::semibold),
        components::HStack(components::HStackOptions{ .spacing = theme::space::sm },
            composition::Element(components::Group())
                .frame({ .width = 8.0F, .height = 8.0F })
                .background(health, theme::Shape::capsule()),
            components::Text(session.console).font(theme::Font::body),
            components::Spacer(),
            components::Text(summary.meta())
                .font(theme::Font::callout)
                .foreground_style(theme::ColourRole::secondary_label)
                .id(composition::ElementKey{ "stream-dock.meta" })),
        controls::Picker(text("akira/stream_menu/panel_stats_title"), std::move(picked),
            [off, compact, full] {
                return composition::Element(components::Group(components::Text(off).tag(StatsOverlayMode::Off),
                    components::Text(compact).tag(StatsOverlayMode::Compact),
                    components::Text(full).tag(StatsOverlayMode::Full)));
            })
            .picker_style(controls::PickerStyle::segmented())
            .labels_hidden(),
        components::HStack(components::HStackOptions{ .spacing = theme::space::sm },
            tile(text("akira/stream_menu/stat_fps"), std::format("{:.1f}", stats.fps), std::string(), fpsTone),
            tile(text("akira/stream_menu/stat_rate"), std::format("{:.1f}", stats.measured_bitrate_mbps), "Mbps", plain),
            tile(text("akira/stream_menu/stat_rtt"), stats.rtt_valid ? std::format("{:.0f}", stats.rtt_ms) : missing, "ms",
                stats.rtt_valid ? tone(stats.rtt_ms, 30.0F, 70.0F) : plain),
            tile(text("akira/stream_menu/stat_loss"), std::format("{:.1f}", stats.packet_loss_percent), "%",
                tone(stats.packet_loss_percent, 0.5F, 2.0F)),
            tile(text("akira/stream_menu/stat_lost"), std::format("{}", stats.network_frames_lost), std::string(), plain),
            tile(text("akira/stream_menu/stat_decode"), commands->decoder(), std::string(), plain))
            .id(composition::ElementKey{ "stream-dock.link" }),
        components::HStack(components::HStackOptions{ .spacing = theme::space::sm },
            tile(text("akira/stream_menu/stat_total"), stats.latency_valid ? std::format("{:.0f}", stats.total_ms) : missing,
                "ms", stats.latency_valid ? tone(stats.total_ms, 50.0F, 100.0F) : plain),
            tile(text("akira/stream_menu/stat_net"), stats.latency_valid ? std::format("{:.0f}", stats.net_ms) : missing, "ms",
                plain),
            tile(text("akira/stream_menu/stat_visual"), std::format("{:.1f}", stats.visual_ms), "ms", plain),
            tile(text("akira/stream_menu/stat_jitter"), std::format("{:.1f}", stats.jitter_ms), "ms", plain),
            tile(text("akira/stream_menu/stat_source"), std::format("{:.1f}", stats.source_fps), std::string(), plain))
            .id(composition::ElementKey{ "stream-dock.latency" }));
}

auto StreamDock::title(StreamControls::Control control) const -> std::string
{
    switch (control)
    {
        case StreamControls::Control::Power: return strings->text("akira/stream_menu/card_power");
        case StreamControls::Control::Stats: return strings->text("akira/stream_menu/card_stats");
        case StreamControls::Control::Pad: return strings->text("akira/stream_menu/card_pad");
        case StreamControls::Control::Players: return strings->text("akira/stream_menu/card_players");
        case StreamControls::Control::Motion: return strings->text("akira/stream_menu/card_motion");
    }
    return {};
}

auto StreamDock::symbol(StreamControls::Control control) -> std::string
{
    switch (control)
    {
        case StreamControls::Control::Power: return "power_settings_new";
        case StreamControls::Control::Stats: return "signal_cellular_alt";
        case StreamControls::Control::Pad: return "sports_esports";
        case StreamControls::Control::Players: return "group";
        case StreamControls::Control::Motion: return "restart_alt";
    }
    return "circle";
}

}
