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
#include <arisa/components/grid_item.hpp>
#include <arisa/components/lazy_vgrid.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/scroll_view.hpp>
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

#include "play/ui/button_board_view.hpp"
#include "play/ui/players_panel.hpp"

namespace akira::play::ui {

namespace async = ::arisa::async;
namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

const composition::StateKey<DiagnosticsReport> StreamDock::sampled{ "stream-dock-stats", DiagnosticsReport{} };
const composition::StateKey<DiagnosticsAppearance> StreamDock::overlay{ "stream-dock-overlay", DiagnosticsAppearance{} };

StreamDock::StreamDock(Session session, StreamCommands& commands, settings::ui::ControlOptions& controlOptions,
    Mapping& mapping, Couch* couch, const app::Strings& strings)
    : session(std::move(session))
    , commands(&commands)
    , controlOptions(&controlOptions)
    , mapping(&mapping)
    , couch(couch)
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
    if (couch && session.players && std::ranges::contains(session.controls, StreamControls::Control::Players))
        shown.push_back(StreamControls::Control::Players);
    if (std::ranges::contains(session.controls, StreamControls::Control::Power))
        shown.push_back(StreamControls::Control::Power);
    return shown;
}

auto StreamDock::opening(const std::vector<StreamControls::Control>& shown) const -> int
{
    if (!session.opening)
        return 0;
    const auto found = std::ranges::find(shown, *session.opening);
    return found == shown.end() ? 0 : static_cast<int>(found - shown.begin());
}

auto StreamDock::body(composition::Context& context) const -> composition::Element
{
    const std::vector<StreamControls::Control> shown = panels();
    const auto chosen = context.state(selected, opening(shown));
    const auto stats = context.state(sampled, commands->diagnostics());
    const auto mode = context.state(overlay, session.diagnostics);
    const auto chosenTopic = context.state(topic);
    const auto board = context.state(mappingShown);
    const settings::ui::ControlOptions::Values& pad = context.read(controlOptions->view());
    const StreamControls::Control current = shown.empty()
        ? StreamControls::Control::Power
        : shown[static_cast<std::size_t>(std::clamp(chosen.get(), 0, static_cast<int>(shown.size()) - 1))];
    auto resume = [commands = commands, board] {
        if (board.get())
            board.set(false);
        else
            commands->resume();
    };
    if (board.get())
        return components::VStack(components::VStackOptions{ .spacing = theme::Space{ dockGap } }, mappingPanel(board))
            .padding(theme::Edge::bottom, theme::Space{ dockBottom })
            .on_exit_command(strings->text("akira/common/back"), resume)
            .id(composition::ElementKey{ "stream-dock" });
    return components::VStack(components::VStackOptions{ .spacing = theme::Space{ dockGap } },
        panel(current, stats.get(), mode, chosenTopic, pad, board), dock(chosen, shown))
        .padding(theme::Edge::bottom, theme::Space{ dockBottom })
        .on_exit_command(strings->text("akira/stream_menu/hint_resume"), resume)
        .task([stats, commands = commands] { return refresh(stats, commands); })
        .id(composition::ElementKey{ "stream-dock" });
}

auto StreamDock::refresh(composition::StateHandle<DiagnosticsReport> stats, StreamCommands* commands) -> async::Task<void>
{
    while (true)
    {
        stats.set(commands->diagnostics());
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
        .id(composition::ElementKey{ "stream-dock.tabs" });
}

auto StreamDock::mappingPanel(composition::StateHandle<bool> board) const -> composition::Element
{
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ panelSpacing } },
        components::HStack(components::HStackOptions{ .spacing = theme::space::sm },
            components::Button([board] { board.set(false); },
                components::Image(components::Image::SystemName{ "arrow_back" }).frame({ .width = 20.0F, .height = 20.0F }))
                .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                .id(composition::ElementKey{ "stream-dock.mapping-back" }),
            components::Text(strings->text("akira/remap/title")).font(theme::Font::title2).font_weight(theme::Font::Weight::semibold)),
        components::ScrollView(ButtonBoardView(*mapping, *strings, false)).frame({ .max_height = mappingHeight }))
        .padding(theme::Space{ panelPadding })
        .frame({ .width = panelWidth, .alignment = composition::Alignment::leading })
        .glass_panel(components::PanelRadius::large)
        .id(composition::ElementKey{ "stream-dock.mapping" });
}

auto StreamDock::panel(StreamControls::Control shown, const DiagnosticsReport& report,
    composition::StateHandle<DiagnosticsAppearance> look, composition::StateHandle<int> chosenTopic,
    const settings::ui::ControlOptions::Values& pad, composition::StateHandle<bool> board) const -> composition::Element
{
    composition::Element content = powerPanel();
    if (shown == StreamControls::Control::Stats)
        content = statsPanel(report, look, chosenTopic);
    else if (shown == StreamControls::Control::Pad)
        content = controlsPanel(pad, board);
    else if (shown == StreamControls::Control::Players && couch)
        content = PlayersPanel(*couch, *strings);
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

auto StreamDock::controlsPanel(const settings::ui::ControlOptions::Values& values, composition::StateHandle<bool> board) const
    -> composition::Element
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
    rows.push_back(components::Button(strings->text("akira/stream_menu/button_mapping"),
        components::Image::SystemName{ "gamepad" }, [board] { board.set(true); })
                       .font(theme::Font::headline)
                       .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                       .control_size(components::ControlSize::large)
                       .button_sizing(components::ButtonSizing::flexible)
                       .id(composition::ElementKey{ "stream-dock.mapping-open" }));
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

auto StreamDock::counter(const DiagnosticsReport::Counter& shown) const -> composition::Element
{
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 5.0F } },
        components::Text(strings->text(shown.label)).font(theme::Font::caption).foreground_style(theme::ColourRole::secondary_label),
        components::Text(std::format("{}", shown.value))
            .font(theme::Font::title3.monospaced_digit())
            .font_weight(theme::Font::Weight::medium))
        .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading })
        .padding(theme::Space{ 12.0F })
        .background(theme::ColourValue(theme::Colour::hex(0xFFFFFF)).opacity(0.06F), theme::Shape::rounded(tileRadius));
}

template <typename Value, std::size_t Count>
auto StreamDock::choice(const std::array<Value, Count>& options, auto&& title,
    composition::StateHandle<DiagnosticsAppearance> look, Value DiagnosticsAppearance::* field, const char* key) const
    -> composition::Element
{
    StreamCommands* sink = commands;
    composition::Binding<Value> picked([look, field] { return look.get().*field; },
        [look, field, sink](Value value) {
            DiagnosticsAppearance next = look.get();
            next.*field = value;
            look.set(next);
            sink->showDiagnostics(next);
        });
    std::vector<std::pair<Value, std::string>> labels;
    for (const Value option : options)
        labels.emplace_back(option, strings->text(title(option)));
    return controls::Picker(strings->text(key), std::move(picked),
        [labels] {
            std::vector<composition::Element> items;
            for (const auto& [option, label] : labels)
                items.push_back(components::Text(label).tag(option));
            return composition::Element(components::Group(std::move(items)));
        })
        .picker_style(controls::PickerStyle::segmented())
        .labels_hidden()
        .id(composition::ElementKey{ std::string("stream-dock.") + key });
}

auto StreamDock::troubleshoot(composition::StateHandle<int> chosenTopic) const -> composition::Element
{
    const int chosen = std::clamp(chosenTopic.get(), 0, static_cast<int>(topics.size()) - 1);
    composition::Binding<int> picked([chosenTopic] { return chosenTopic.get(); }, [chosenTopic](int value) { chosenTopic.set(value); });
    std::vector<std::string> labels;
    for (const char* key : topics)
        labels.push_back(strings->text(key));
    std::vector<composition::Element> tips;
    tips.push_back(components::Text(strings->text(std::format("akira/diagnostics/tips_{}_heading", chosen)))
                       .font(theme::Font::subheadline)
                       .font_weight(theme::Font::Weight::semibold));
    for (int index = 0; index < tipCount(chosen); ++index)
        tips.push_back(components::Text(strings->text(std::format("akira/diagnostics/tips_{}_{}", chosen, index)))
                           .font(theme::Font::callout)
                           .foreground_style(theme::ColourRole::secondary_label));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ panelSpacing } },
        components::Text(strings->text("akira/diagnostics/connection_tips")).font(theme::Font::headline),
        controls::Picker(strings->text("akira/diagnostics/connection_tips"), std::move(picked),
            [labels] {
                std::vector<composition::Element> items;
                for (std::size_t index = 0; index < labels.size(); ++index)
                    items.push_back(components::Text(labels[index]).tag(static_cast<int>(index)));
                return composition::Element(components::Group(std::move(items)));
            })
            .picker_style(controls::PickerStyle::segmented())
            .labels_hidden()
            .id(composition::ElementKey{ "stream-dock.topics" }),
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                               .spacing = theme::Space{ 10.0F } },
            std::move(tips)));
}

auto StreamDock::tipCount(int chosen) -> int
{
    constexpr std::array counts{ 4, 2, 2 };
    return counts[static_cast<std::size_t>(chosen)];
}

auto StreamDock::levelKey(DiagnosticsAppearance::Level level) -> const char*
{
    switch (level)
    {
        case DiagnosticsAppearance::Level::Off: return "akira/diagnostics/level_off";
        case DiagnosticsAppearance::Level::Compact: return "akira/diagnostics/level_compact";
        case DiagnosticsAppearance::Level::Standard: return "akira/diagnostics/level_standard";
        case DiagnosticsAppearance::Level::Full: return "akira/diagnostics/level_full";
    }
    return "akira/diagnostics/level_off";
}

auto StreamDock::glassKey(DiagnosticsAppearance::Glass glass) -> const char*
{
    switch (glass)
    {
        case DiagnosticsAppearance::Glass::Regular: return "akira/diagnostics/glass_regular";
        case DiagnosticsAppearance::Glass::Clear: return "akira/diagnostics/glass_clear";
        case DiagnosticsAppearance::Glass::ClearDimmed: return "akira/diagnostics/glass_dimmed";
        case DiagnosticsAppearance::Glass::HealthTint: return "akira/diagnostics/glass_health";
        case DiagnosticsAppearance::Glass::Solid: return "akira/diagnostics/glass_solid";
    }
    return "akira/diagnostics/glass_regular";
}

auto StreamDock::layoutKey(DiagnosticsAppearance::Layout layout) -> const char*
{
    return layout == DiagnosticsAppearance::Layout::Floating ? "akira/diagnostics/layout_floating"
                                                             : "akira/diagnostics/layout_sheet";
}

auto StreamDock::statsPanel(const DiagnosticsReport& report, composition::StateHandle<DiagnosticsAppearance> look,
    composition::StateHandle<int> chosenTopic) const -> composition::Element
{
    std::vector<composition::Element> rows;
    rows.push_back(components::Text(strings->text("akira/diagnostics/title"))
                       .font(theme::Font::title2)
                       .font_weight(theme::Font::Weight::semibold));
    if (session.datacenter)
        rows.push_back(components::LabeledContent(strings->text("akira/diagnostics/datacenter"), *session.datacenter));
    if (session.bandwidth)
        rows.push_back(components::LabeledContent(strings->text("akira/diagnostics/bandwidth"), *session.bandwidth));
    rows.push_back(choice(DiagnosticsAppearance::levels, &StreamDock::levelKey, look, &DiagnosticsAppearance::level,
        "akira/diagnostics/level"));
    rows.push_back(choice(DiagnosticsAppearance::glasses, &StreamDock::glassKey, look, &DiagnosticsAppearance::glass,
        "akira/diagnostics/glass"));
    rows.push_back(choice(DiagnosticsAppearance::layouts, &StreamDock::layoutKey, look, &DiagnosticsAppearance::layout,
        "akira/diagnostics/layout"));
    std::vector<composition::Element> tiles;
    for (const DiagnosticsReport::Counter& shown : report.counters())
        tiles.push_back(counter(shown));
    rows.push_back(components::LazyVGrid(
        components::LazyVGridOptions{
            .columns = { components::GridItem{ components::GridItem::Size::adaptive(tileMinimum), theme::Space{ tileSpacing } } },
            .spacing = theme::Space{ tileSpacing } },
        std::move(tiles))
                       .id(composition::ElementKey{ "stream-dock.counters" }));
    rows.push_back(troubleshoot(chosenTopic));
    return components::ScrollView(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                                         .spacing = theme::Space{ panelSpacing } },
                                      std::move(rows)))
        .frame({ .max_height = diagnosticsHeight });
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
