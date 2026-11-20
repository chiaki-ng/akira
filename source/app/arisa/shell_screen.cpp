#include "app/arisa/shell_screen.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/presentation/alert.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/structure/navigation_split_view.hpp>
#include <arisa/structure/scaffold.hpp>
#include <arisa/structure/tab.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/structure/tab_view_style.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <chrono>
#include <format>
#include <utility>
#include <vector>

#include "app/version.hpp"
#include "consoles/ui/console_grid.hpp"
#include "settings/ui/settings_screen.hpp"

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

ShellScreen::ShellScreen(ShellModel& model, Notices& notices, const Parts& parts, const app::Strings& strings)
    : model(&model)
    , notices(&notices)
    , parts(&parts)
    , strings(&strings)
{
}

auto ShellScreen::versionText() -> std::string
{
    return std::format("akira {}", app::version::string());
}

auto ShellScreen::body(composition::Context& context) const -> composition::Element
{
    const ShellModel::View& view = context.read(model->view());
    static_cast<void>(context.read(notices->view()));
    if (view.phase == ShellModel::Phase::Starting)
        return components::Text(versionText())
            .font(theme::Font::footnote)
            .foreground_style(theme::ColourRole::tertiary_label)
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
            .background(theme::ColourRole::background);
    const auto chosen = context.state(section);
    composition::Element content = announced(tabs(chosen), view.phase == ShellModel::Phase::Streaming)
                                       .toolbar(composition::ToolbarItem(composition::ToolbarItemPlacement::top_bar_trailing,
                                           components::Text(versionText())
                                               .font(theme::Font::footnote)
                                               .foreground_style(theme::ColourRole::secondary_label)));
    composition::Element shell =
        ending(composition::Element(structure::Scaffold(std::move(content))).background(theme::ColourRole::background),
            view);
    const bool connecting = view.phase == ShellModel::Phase::Connecting;
    composition::Element loading = components::Group();
    if (connecting)
        loading = play::ui::ConnectingScreen(view.progress, *parts->stream, *strings);
    return std::move(shell).overlay(connecting, composition::OverlayLayer::sheet, std::move(loading));
}

auto ShellScreen::tabs(composition::StateHandle<Section> chosen) const -> composition::Element
{
    const Parts* held = parts;
    const app::Strings* text = strings;
    std::vector<composition::Element> pages;
    pages.push_back(structure::Tab(strings->text("akira/tabs/consoles"), "sports_esports", Section::Consoles, [held, text] {
        return composition::Element(consoles::ui::ConsoleGrid(*held->shelf, *held->consoles, *text));
    }));
    pages.push_back(structure::Tab(strings->text("akira/tabs/settings"), "settings", Section::Settings, [held, text] {
        return composition::Element(
            settings::ui::SettingsScreen(*held->picture, *held->controls, *held->nat, *held->help, *text));
    }));
    return composition::Element(structure::TabView(composition::Binding<Section>(chosen), std::move(pages)))
        .tab_view_style(structure::SidebarAdaptableTabViewStyle{})
        .sidebar_collapse(structure::SidebarCollapse{ .detail_only = true, .icons_only = true });
}

auto ShellScreen::announced(composition::Element content, bool streaming) const -> composition::Element
{
    Notices* held = notices;
    composition::Binding<std::optional<Notices::Notice>> shown([held] { return held->view().get(); },
        [held](std::optional<Notices::Notice> value) {
            if (!value)
                held->clear();
        });
    const theme::Glass glass = streaming ? theme::Glass::regular().tint(theme::ColourRole::orange) : theme::Glass::regular();
    return std::move(content).toast(std::move(shown), std::chrono::seconds{ 3 }, [glass](const Notices::Notice& notice) {
        return components::Text(notice.text)
            .font(theme::Font::callout)
            .padding(theme::Insets::symmetric(theme::space::lg, theme::space::md))
            .glass_effect(glass, theme::Shape::capsule());
    });
}

auto ShellScreen::ending(composition::Element content, const ShellModel::View& view) const -> composition::Element
{
    ShellModel* held = model;
    play::ui::StreamCommands* stream = parts->stream;
    const ShellModel::Ending shown = view.ending.value_or(ShellModel::Ending{});
    composition::Binding<bool> presented([held] { return held->view().get().ending.has_value(); },
        [held](bool open) {
            if (!open)
                held->dismissEnding();
        });
    return std::move(content).alert(shown.title, std::move(presented),
        [held, stream, reconnect = shown.reconnect, ok = strings->text("akira/common/ok"),
            yes = strings->text("akira/stream/yes"), no = strings->text("akira/stream/no")] {
            if (!reconnect)
                return composition::Element(components::Button(ok, components::ButtonRole::cancel,
                    [held] { held->dismissEnding(); }));
            return composition::Element(components::Group(
                components::Button(no, components::ButtonRole::cancel, [held] { held->dismissEnding(); }),
                components::Button(yes, [held, stream] {
                    held->dismissEnding();
                    stream->reconnect();
                })));
        },
        [message = shown.message] { return components::Text(message); });
}

}
