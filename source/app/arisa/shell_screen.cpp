#include "app/arisa/shell_screen.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_border_shape.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/glass_panel.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hint_bar.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/presentation/alert.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <chrono>
#include <format>
#include <utility>

#include "app/arisa/header.hpp"
#include "app/arisa/rail.hpp"
#include "app/version.hpp"
#include "consoles/ui/console_grid.hpp"
#include "play/ui/connecting_screen.hpp"
#include "settings/ui/settings_screen.hpp"

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace graphics = ::arisa::graphics;
namespace motion = ::arisa::motion;
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
            .background(theme::ColourRole::window_background);
    const bool connecting = view.phase == ShellModel::Phase::Connecting;
    const bool settings = view.settings && view.phase == ShellModel::Phase::Home;
    composition::Element shell =
        ending(announced(home(), view.phase == ShellModel::Phase::Streaming), view);
    composition::Element panel = components::Group();
    if (settings)
        panel = settingsPanel(context.environment(components::Environment::theme).glass().panel);
    composition::Element loading = components::Group();
    if (connecting)
        loading = play::ui::ConnectingScreen(view.progress, *parts->stream, *strings);
    return std::move(shell)
        .overlay(settings, composition::OverlayLayer::sheet, std::move(panel))
        .overlay(connecting, composition::OverlayLayer::sheet, std::move(loading));
}

auto ShellScreen::home() const -> composition::Element
{
    return components::ZStack(components::ZStackOptions{ .alignment = composition::Alignment::top_leading },
        backdrop(), page(), composition::Element(Rail(*model, *strings))
            .padding(theme::Space{ Rail::inset })
            .frame({ .max_height = composition::infinity, .alignment = composition::Alignment::top_leading }),
        hints());
}

auto ShellScreen::backdrop() -> composition::Element
{
    const graphics::Gradient glow(std::vector<theme::ColourValue>{
        theme::ColourValue(theme::ColourRole::accent).opacity(glowOpacity),
        theme::ColourValue(theme::ColourRole::accent).opacity(0.0F) });
    return composition::Element(components::Group())
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .background(graphics::ShapeStyle(graphics::EllipticalGradient{ .gradient = glow,
            .center = motion::UnitPoint{ 0.08F, 0.9F },
            .end_radius_fraction = 0.7F }))
        .background(theme::ColourRole::window_background)
        .id(composition::ElementKey{ "shell.backdrop" });
}

auto ShellScreen::page() const -> composition::Element
{
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ pageSpacing } },
        Header(strings->text("akira/tabs/consoles")),
        composition::Element(consoles::ui::ConsoleGrid(*parts->shelf, *parts->consoles, *strings))
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
                .alignment = composition::Alignment::top_leading }))
        .padding(theme::Insets{ .top = theme::Space{ pageTop },
            .leading = theme::Space{ pageLeading },
            .bottom = theme::Space{ pageBottom },
            .trailing = theme::Space{ pageTrailing } })
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
            .alignment = composition::Alignment::top_leading })
        .id(composition::ElementKey{ "shell.page" });
}

auto ShellScreen::hints() -> composition::Element
{
    return composition::Element(components::HintBar())
        .foreground_style(theme::ColourRole::secondary_label)
        .padding(theme::Insets{ .bottom = theme::Space{ hintsBottom }, .trailing = theme::Space{ hintsTrailing } })
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
            .alignment = composition::Alignment::bottom_trailing })
        .id(composition::ElementKey{ "shell.hints" });
}

auto ShellScreen::settingsPanel(theme::Glass glass) const -> composition::Element
{
    ShellModel* held = model;
    settings::ui::SettingsModel* settingsModel = parts->settings.model;
    auto close = [held, settingsModel] {
        if (settingsModel)
            settingsModel->close();
        held->showSettings(false);
    };
    composition::Element heading =
        components::HStack(components::HStackOptions{ .spacing = theme::Space{ 10.0F } },
            components::Image(components::Image::SystemName{ "settings" })
                .foreground_style(theme::ColourRole::secondary_label),
            components::Text(strings->text("akira/tabs/settings")).font(theme::Font::headline),
            components::Spacer(),
            components::Button(close,
                components::Image(components::Image::SystemName{ "close" }).frame({ .width = 22.0F, .height = 22.0F }))
                .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                .button_border_shape(components::ButtonBorderShape::circle())
                .id(composition::ElementKey{ "settings.close" }))
            .padding(theme::Insets{ .top = theme::Space{ 16.0F }, .leading = theme::Space{ 22.0F },
                .trailing = theme::Space{ 18.0F } });
    composition::Element panel =
        components::VStack(components::VStackOptions{ .spacing = theme::space::none }, std::move(heading),
            composition::Element(settings::ui::SettingsScreen(parts->settings, *strings))
                .frame({ .max_width = composition::infinity, .max_height = composition::infinity }))
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
            .glass_effect(glass, components::GlassPanel::shape(components::PanelRadius::large));
    return components::VStack(components::VStackOptions{ .spacing = theme::space::none }, std::move(panel))
        .padding(theme::Insets{ .top = theme::Space{ settingsTop },
            .leading = theme::Space{ settingsSide },
            .bottom = theme::Space{ settingsBottom },
            .trailing = theme::Space{ settingsSide } })
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .background(theme::ColourValue(theme::Colour::hex(0x000000)).opacity(settingsScrim))
        .on_exit_command(strings->text("akira/common/close"), close)
        .id(composition::ElementKey{ "settings.panel" });
}

auto ShellScreen::announced(composition::Element content, bool streaming) const -> composition::Element
{
    Notices* held = notices;
    composition::Binding<std::optional<Notices::Notice>> shown([held] { return held->view().get(); },
        [held](std::optional<Notices::Notice> value) {
            if (!value)
                held->clear();
        });
    return std::move(content).toast(std::move(shown), std::chrono::seconds{ 3 }, [streaming](const Notices::Notice& notice) {
        composition::Element text = components::Text(notice.text).font(theme::Font::subheadline);
        if (!streaming)
            return text;
        return std::move(text)
            .padding(theme::Insets::symmetric(theme::Space{ 16.0F }, theme::Space{ 9.0F }))
            .glass_effect(theme::Glass::regular().tint(theme::ColourRole::orange), theme::Shape::capsule());
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
