#include "play/ui/stream_dock.hpp"

#include <arisa/async/awaitables.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_sizing.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/divider.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <chrono>
#include <format>
#include <utility>

#include "play/stream_summary.hpp"

namespace akira::play::ui {

namespace async = ::arisa::async;
namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace theme = ::arisa::theme;

const composition::StateKey<StreamStats> StreamDock::sampled{ "stream-dock-stats", StreamStats{} };
const composition::StateKey<StatsOverlayMode> StreamDock::overlay{ "stream-dock-overlay", StatsOverlayMode::Off };

StreamDock::StreamDock(Session session, StreamCommands& commands, const app::Strings& strings)
    : session(std::move(session))
    , commands(&commands)
    , strings(&strings)
{
}

auto StreamDock::offered(StreamControls::Control control) -> bool
{
    return control == StreamControls::Control::Power || control == StreamControls::Control::Stats
        || control == StreamControls::Control::Motion;
}

auto StreamDock::body(composition::Context& context) const -> composition::Element
{
    const auto chosen = context.state(selected);
    const auto stats = context.state(sampled);
    const auto mode = context.state(overlay, session.statsMode);
    std::vector<StreamControls::Control> panels;
    std::ranges::copy_if(session.controls, std::back_inserter(panels), [](StreamControls::Control control) {
        return offered(control) && control != StreamControls::Control::Motion;
    });
    const StreamControls::Control shown = panels.empty()
        ? StreamControls::Control::Power
        : panels[static_cast<std::size_t>(std::clamp(chosen.get(), 0, static_cast<int>(panels.size()) - 1))];
    auto resume = [commands = commands] { commands->resume(); };
    return components::VStack(components::VStackOptions{ .spacing = theme::space::md },
        panel(shown, stats.get(), mode), tabs(chosen, shown))
        .padding(theme::Edge::bottom, theme::Space{ 24.0F })
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

auto StreamDock::tabs(composition::StateHandle<int> chosen, StreamControls::Control shown) const
    -> composition::Element
{
    std::vector<composition::Element> items;
    int panelIndex = 0;
    for (const StreamControls::Control control : session.controls)
    {
        if (!offered(control))
            continue;
        const bool instant = control == StreamControls::Control::Motion;
        const int index = instant ? -1 : panelIndex++;
        composition::Element label = components::VStack(components::VStackOptions{ .spacing = theme::space::xxs },
            components::Image(components::Image::SystemName{ symbol(control) }),
            components::Text(title(control)).font(theme::Font::caption));
        composition::Element tab = components::Button(
            [chosen, index, instant, commands = commands] {
                if (instant)
                {
                    commands->resetMotion();
                    commands->resume();
                    return;
                }
                chosen.set(index);
            },
            std::move(label))
                                       .button_style(components::ButtonStyle::plain())
                                       .frame({ .width = tabWidth, .height = tabHeight })
                                       .id(composition::ElementKey{ "stream-dock.tab." + symbol(control) });
        if (!instant && control == shown)
            tab = std::move(tab).background(theme::ColourRole::quaternary_system_fill, theme::Shape::capsule());
        items.push_back(std::move(tab));
    }
    items.push_back(components::Divider().frame({ .height = 32.0F }));
    items.push_back(components::Button([commands = commands] { commands->resume(); },
        components::Image(components::Image::SystemName{ "close" }))
                        .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                        .frame({ .width = tabHeight, .height = tabHeight })
                        .id(composition::ElementKey{ "stream-dock.resume" }));
    return components::HStack(components::HStackOptions{ .spacing = theme::space::xs }, std::move(items))
        .padding(theme::Space{ 6.0F })
        .glass_effect(theme::Glass::regular(), theme::Shape::capsule())
        .focus_section();
}

auto StreamDock::panel(StreamControls::Control shown, const StreamStats& stats,
    composition::StateHandle<StatsOverlayMode> mode) const -> composition::Element
{
    composition::Element content =
        shown == StreamControls::Control::Stats ? statsPanel(stats, mode) : powerPanel();
    return std::move(content)
        .padding(theme::Space{ panelPadding })
        .frame({ .max_width = panelWidth, .alignment = composition::Alignment::leading })
        .glass_effect(theme::Glass::regular(), theme::Shape::rounded(28.0F));
}

auto StreamDock::powerPanel() const -> composition::Element
{
    std::vector<composition::Element> rows;
    rows.push_back(components::Text(strings->text("akira/stream_menu/panel_power_title")).font(theme::Font::headline));
    rows.push_back(components::Text(session.console)
                       .font(theme::Font::subheadline)
                       .foreground_style(theme::ColourRole::secondary_label));
    rows.push_back(components::Button(strings->text("akira/stream_menu/opt_disconnect"),
        [commands = commands] { commands->leave(false); })
                       .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                       .button_sizing(components::ButtonSizing::flexible)
                       .id(composition::ElementKey{ "stream-dock.disconnect" }));
    if (session.sleepOffered)
        rows.push_back(components::Button(strings->text("akira/stream_menu/opt_disconnect_sleep"),
            components::ButtonRole::destructive, [commands = commands] { commands->leave(true); })
                           .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                           .button_sizing(components::ButtonSizing::flexible)
                           .id(composition::ElementKey{ "stream-dock.sleep" }));
    return components::VStack(
        components::VStackOptions{ .alignment = components::HorizontalAlignment::leading, .spacing = theme::space::md },
        std::move(rows));
}

auto StreamDock::statsPanel(const StreamStats& stats, composition::StateHandle<StatsOverlayMode> mode) const
    -> composition::Element
{
    const StreamSummary summary(stats);
    theme::ColourValue tone = theme::ColourRole::green;
    if (summary.health() == StreamSummary::Health::Fair)
        tone = theme::ColourRole::orange;
    else if (summary.health() == StreamSummary::Health::Poor)
        tone = theme::ColourRole::red;
    const std::string rtt = stats.rtt_valid ? std::format("{:.0f} ms", stats.rtt_ms) : std::string("-");
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
    return components::VStack(
        components::VStackOptions{ .alignment = components::HorizontalAlignment::leading, .spacing = theme::space::md },
        components::HStack(components::HStackOptions{ .spacing = theme::space::sm },
            composition::Element(components::Group())
                .frame({ .width = 10.0F, .height = 10.0F })
                .background(tone, theme::Shape::capsule()),
            components::Text(session.console).font(theme::Font::headline),
            components::Spacer(),
            components::Text(summary.meta())
                .font(theme::Font::subheadline)
                .foreground_style(theme::ColourRole::secondary_label)
                .id(composition::ElementKey{ "stream-dock.meta" })),
        controls::Picker(strings->text("akira/stream_menu/panel_stats_title"), std::move(picked),
            [off, compact, full] {
                return composition::Element(components::Group(components::Text(off).tag(StatsOverlayMode::Off),
                    components::Text(compact).tag(StatsOverlayMode::Compact),
                    components::Text(full).tag(StatsOverlayMode::Full)));
            })
            .picker_style(controls::PickerStyle::segmented()),
        components::LabeledContent(strings->text("akira/stream_menu/stat_fps"), std::format("{:.1f}", stats.fps)),
        components::LabeledContent(strings->text("akira/stream_menu/stat_rate"),
            std::format("{:.1f} Mbps", stats.measured_bitrate_mbps)),
        components::LabeledContent(strings->text("akira/stream_menu/stat_rtt"), rtt),
        components::LabeledContent(strings->text("akira/stream_menu/stat_loss"),
            std::format("{:.1f}%", stats.packet_loss_percent)),
        components::LabeledContent(strings->text("akira/stream_menu/stat_lost"),
            std::format("{}", stats.network_frames_lost)));
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
        case StreamControls::Control::Stats: return "monitoring";
        case StreamControls::Control::Pad: return "sports_esports";
        case StreamControls::Control::Players: return "group";
        case StreamControls::Control::Motion: return "screen_rotation";
    }
    return "circle";
}

}
