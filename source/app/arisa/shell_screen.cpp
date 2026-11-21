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
#include <arisa/composition/toast_options.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <chrono>
#include <format>
#include <utility>
#include <vector>

#include "accounts/ui/account_flow.hpp"
#include "app/arisa/confirmation.hpp"
#include "app/arisa/header.hpp"
#include "app/arisa/notice_toast.hpp"
#include "app/arisa/rail.hpp"
#include "app/version.hpp"
#include "consoles/ui/console_grid.hpp"
#include "trophies/ui/trophies_screen.hpp"
#if AKIRA_CLOUD
#include "cloud/ui/cloud_screen.hpp"
#endif
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
    composition::Element shell = exitCard(
        ending(announced(home(view.page), view.phase == ShellModel::Phase::Streaming), view, context.namespace_id(endingFocus)),
        view);
    composition::Element panel = components::Group();
    if (settings)
        panel = settingsPanel(context.environment(components::Environment::theme).glass().panel);
    composition::Element loading = components::Group();
    if (connecting)
        loading = play::ui::ConnectingScreen(view.progress, *parts->stream, *strings);
    accounts::ui::AccountModel* profiles = parts->settings.accounts;
    const bool onboarding = profiles && view.phase == ShellModel::Phase::Home
        && context.read(profiles->view()).flow != accounts::ui::AccountModel::Flow::None;
    composition::Element flow = components::Group();
    if (onboarding)
        flow = accounts::ui::AccountFlow(*profiles, *strings);
    composition::Element shown = std::move(shell)
                                     .overlay(settings, composition::OverlayLayer::sheet, std::move(panel))
                                     .overlay(connecting, composition::OverlayLayer::sheet, std::move(loading))
                                     .overlay(onboarding, composition::OverlayLayer::sheet, std::move(flow));
    if (parts->art)
        return std::move(shown).async_image_url_session(parts->art->session());
    return shown;
}

auto ShellScreen::home(ShellModel::Page shown) const -> composition::Element
{
    ShellModel* held = model;
    return components::ZStack(components::ZStackOptions{ .alignment = composition::Alignment::top_leading },
        backdrop(), page(shown), composition::Element(Rail(*model, *strings))
            .padding(theme::Space{ Rail::inset })
            .frame({ .max_height = composition::infinity, .alignment = composition::Alignment::top_leading }),
        hints())
        .on_exit_command(strings->text("akira/app/exit"), [held] { held->askExit(); });
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

auto ShellScreen::page(ShellModel::Page shown) const -> composition::Element
{
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ pageSpacing } },
        Header(pageTitle(shown), parts->settings.accounts, *strings),
        pageContent(shown).frame({ .max_width = composition::infinity, .max_height = composition::infinity,
            .alignment = composition::Alignment::top_leading }))
        .padding(theme::Insets{ .top = theme::Space{ pageTop },
            .leading = theme::Space{ pageLeading },
            .bottom = theme::Space{ pageBottom },
            .trailing = theme::Space{ pageTrailing } })
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
            .alignment = composition::Alignment::top_leading })
        .id(composition::ElementKey{ "shell.page" });
}

auto ShellScreen::pageTitle(ShellModel::Page shown) const -> std::string
{
    switch (shown)
    {
        case ShellModel::Page::Cloud: return strings->text("akira/cloud/card_title");
        case ShellModel::Page::Trophies: return strings->text("akira/tabs/trophies");
        case ShellModel::Page::Consoles: break;
    }
    return strings->text("akira/tabs/consoles");
}

auto ShellScreen::pageContent(ShellModel::Page shown) const -> composition::Element
{
#if AKIRA_CLOUD
    if (shown == ShellModel::Page::Cloud && parts->cloud)
        return composition::Element(cloud::ui::CloudScreen(*parts->cloud, *strings)).id(composition::ElementKey{ "shell.cloud" });
#endif
    if (shown == ShellModel::Page::Trophies && parts->trophies)
        return composition::Element(trophies::ui::TrophiesScreen(*parts->trophies, *strings))
            .id(composition::ElementKey{ "shell.trophies" });
    return composition::Element(consoles::ui::ConsoleGrid(*parts->shelf, *parts->consoles, parts->recent, *strings))
        .id(composition::ElementKey{ "shell.consoles" });
}

auto ShellScreen::hints() -> composition::Element
{
    return composition::Element(components::HintBar())
        .foreground_style(theme::ColourRole::secondary_label)
        .padding(theme::Insets{ .bottom = theme::Space{ hintsBottom }, .trailing = theme::Space{ hintsTrailing } })
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
            .alignment = composition::Alignment::bottom_trailing })
        .allows_hit_testing(false)
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
    if (streaming)
        return content;
    return NoticeToast::attach(std::move(content), *notices, false);
}

auto ShellScreen::ending(composition::Element content, const ShellModel::View& view, composition::Namespace::ID focus) const
    -> composition::Element
{
    if (!view.ending)
        return content;
    return std::move(content).overlay(true, composition::OverlayLayer::dialog, endingCard(*view.ending, focus));
}

auto ShellScreen::exitCard(composition::Element content, const ShellModel::View& view) const -> composition::Element
{
    if (!view.exiting)
        return content;
    ShellModel* held = model;
    std::vector<Confirmation::Choice> choices;
    choices.push_back(Confirmation::Choice{ .label = strings->text("akira/common/cancel"),
        .chosen = [held] { held->dismissExit(); },
        .key = "exit.cancel" });
    choices.push_back(Confirmation::Choice{ .label = strings->text("akira/app/exit_confirm"),
        .chosen = [held] { held->confirmExit(); },
        .emphasis = Confirmation::Emphasis::Destructive,
        .key = "exit.confirm" });
    return std::move(content).overlay(true, composition::OverlayLayer::dialog,
        Confirmation(strings->text("akira/app/exit_title"), std::string(), std::move(choices), [held] { held->dismissExit(); })
            .dimmed("exit"));
}

auto ShellScreen::endingCard(const ShellModel::Ending& shown, composition::Namespace::ID focus) const -> composition::Element
{
    ShellModel* held = model;
    play::ui::StreamCommands* stream = parts->stream;
    auto dismiss = [held] { held->dismissEnding(); };
    std::vector<Confirmation::Choice> choices;
    if (shown.reconnect)
    {
        choices.push_back(Confirmation::Choice{ .label = strings->text("akira/stream/no"), .chosen = dismiss, .key = "ending.no" });
        choices.push_back(Confirmation::Choice{ .label = strings->text("akira/stream/yes"),
            .chosen = [held, stream] {
                held->dismissEnding();
                stream->reconnect();
            },
            .emphasis = Confirmation::Emphasis::Prominent,
            .key = "ending.yes" });
    }
    else
        choices.push_back(Confirmation::Choice{ .label = strings->text("akira/common/ok"),
            .chosen = dismiss,
            .emphasis = Confirmation::Emphasis::Prominent,
            .key = "ending.ok" });
    return Confirmation(shown.title, shown.message, std::move(choices), dismiss).focusing(focus).dimmed("ending");
}

}
