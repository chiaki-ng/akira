#include "app/app_settings.hpp"
#include "app/arisa/shell_screen.hpp"

#include <arisa/provider/input/buttons.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_border_shape.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/open_url_action.hpp>
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

#include <algorithm>
#include <chrono>
#include <format>
#include <functional>
#include <utility>
#include <vector>

#include "accounts/ui/account_flow.hpp"
#include "consoles/ui/recent_shelf.hpp"
#include "app/arisa/ambient_background.hpp"
#include "app/arisa/brand_lockup.hpp"
#include "app/arisa/confirmation.hpp"
#include "app/arisa/link_code.hpp"
#include "app/arisa/header.hpp"
#include "network/ui/network_facts_row.hpp"
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
namespace provider = ::arisa::provider;
namespace graphics = ::arisa::graphics;
namespace motion = ::arisa::motion;
namespace theme = ::arisa::theme;

const composition::StateKey<std::string> ShellScreen::linkShown{ "shell-shown-link", std::string() };

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
    if (view.phase == ShellModel::Phase::Restricted)
        return appletNotice();
    const bool connecting = view.phase == ShellModel::Phase::Connecting;
    const bool settings = view.settings && view.phase == ShellModel::Phase::Home;
    composition::Element shell = dnsCard(exitCard(ending(announced(home(view, railEdge(context), settings), view.phase == ShellModel::Phase::Streaming),
                                                      view, context.namespace_id(endingFocus)),
                                             view),
        view, context.namespace_id(dnsFocus));
    composition::Element panel = components::Group();
    if (settings)
        panel = settingsPanel(railEdge(context));
    composition::Element loading = components::Group();
    if (connecting)
        loading = play::ui::ConnectingScreen(view.progress, *parts->stream, *strings, loadingArt(context, view.progress.console));
    accounts::ui::AccountModel* profiles = parts->settings.accounts;
    const bool onboarding = profiles && view.phase == ShellModel::Phase::Home
        && context.read(profiles->view()).flow != accounts::ui::AccountModel::Flow::None;
    composition::Element flow = components::Group();
    if (onboarding)
        flow = accounts::ui::AccountFlow(*profiles, *strings);
    composition::Element shown = linking(std::move(shell)
                                             .overlay(settings, composition::OverlayLayer::sheet, std::move(panel))
                                             .overlay(connecting, composition::OverlayLayer::sheet, std::move(loading))
                                             .overlay(onboarding, composition::OverlayLayer::sheet, std::move(flow)),
        context);
    if (parts->art)
        return std::move(shown).async_image_url_session(parts->art->session());
    return shown;
}

auto ShellScreen::linking(composition::Element content, composition::Context& context) const -> composition::Element
{
    const auto link = context.state(linkShown);
    const std::string url = link.get();
    return std::move(content)
        .overlay(!url.empty(), composition::OverlayLayer::dialog,
            LinkCode(url, [link] { link.set(std::string()); }).dimmed(*strings))
        .environment(components::Environment::open_url,
            components::OpenURLAction([link](const provider::Url& opened) {
                link.set(std::string(opened.value()));
                return components::OpenURLAction::Result::handled;
            }));
}

auto ShellScreen::loadingArt(composition::Context& context, const std::string& console) const
    -> std::vector<play::ui::ConnectingScreen::Art>
{
    std::vector<play::ui::ConnectingScreen::Art> arts;
    if (!parts->recent)
        return arts;
    const consoles::ui::RecentShelf::View& shown = context.read(parts->recent->view());
    for (const consoles::ui::RecentShelf::Banner& banner : parts->recent->loadingArt(console))
    {
        const auto title = std::ranges::find(shown.titles, banner.titleId, &consoles::RecentPlay::Title::titleId);
        arts.push_back(play::ui::ConnectingScreen::Art{ .title = title == shown.titles.end() ? std::string{} : title->name,
            .background = banner.background,
            .hero = banner.hero,
            .logo = banner.logo });
    }
    return arts;
}

auto ShellScreen::railEdge(composition::Context& context) const -> RailEdge
{
    if (!parts->settings.model)
        return RailEdge::Left;
    static_cast<void>(context.read(parts->settings.model->view()));
    return parts->settings.model->parts().app.railEdge();
}

auto ShellScreen::home(const ShellModel::View& view, RailEdge edge, bool sheet) const -> composition::Element
{
    const ShellModel::Page shown = view.page;
    ShellModel* held = model;
    composition::Element rail = Rail(*model, *strings, parts->settings.accounts, edge);
    std::vector<composition::Element> layers;
    layers.push_back(backdrop());
    layers.push_back(page(view, edge));
    if (edge != RailEdge::Bottom && !sheet)
        layers.push_back(composition::Element(network::ui::NetworkFactsRow(view.network, *strings))
                .padding(theme::Insets{ .leading = theme::Space{ hintsTrailing },
                    .bottom = theme::Space{ hintsBottom + railBand(edge) } })
                .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
                    .alignment = composition::Alignment::bottom_leading }));
    layers.push_back(std::move(rail));
    layers.push_back(hints(edge));
    return components::ZStack(components::ZStackOptions{ .alignment = composition::Alignment::top_leading }, std::move(layers))
        .on_command(provider::Button::y, strings->text("akira/rail/menu"), [held] { held->callRail(); })
        .on_exit_command(strings->text("akira/app/exit"), [held] { held->askExit(); });
}

auto ShellScreen::backdrop() -> composition::Element
{
    const graphics::Gradient glow(std::vector<theme::ColourValue>{
        theme::ColourValue(theme::ColourRole::accent).opacity(glowOpacity),
        theme::ColourValue(theme::ColourRole::accent).opacity(0.0F) });
    return composition::Element(AmbientBackground(AmbientBackground::Presence::Quiet))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .background(graphics::ShapeStyle(graphics::EllipticalGradient{ .gradient = glow,
            .center = motion::UnitPoint{ 0.08F, 0.9F },
            .end_radius_fraction = 0.7F }))
        .background(theme::ColourRole::window_background)
        .id(composition::ElementKey{ "shell.backdrop" });
}

auto ShellScreen::page(const ShellModel::View& view, RailEdge edge) const -> composition::Element
{
    const ShellModel::Page shown = view.page;
    const float railSide = edge == RailEdge::Right ? pageLeading - pageTrailing : 0.0F;
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ pageSpacing } },
        Header(pageTitle(shown), *strings, edge == RailEdge::Bottom ? std::optional(view.network) : std::nullopt),
        pageContent(shown)
            .padding(theme::Edge::trailing, theme::Space{ railSide })
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
                .alignment = composition::Alignment::top_leading }))
        .padding(theme::Insets{ .top = theme::Space{ edge == RailEdge::Top ? pageTopUnderBar : pageTop },
            .leading = theme::Space{ edge == RailEdge::Left ? pageLeading : pageTrailing },
            .bottom = theme::Space{ edge == RailEdge::Bottom ? pageBottomOverBar : pageBottom },
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

auto ShellScreen::railBand(RailEdge edge) -> float
{
    return edge == RailEdge::Bottom ? Rail::barHeight + Rail::inset : 0.0F;
}

auto ShellScreen::hints(RailEdge edge) -> composition::Element
{
    return composition::Element(components::HintBar())
        .foreground_style(theme::ColourRole::secondary_label)
        .padding(theme::Insets{ .bottom = theme::Space{ hintsBottom + railBand(edge) }, .trailing = theme::Space{ hintsTrailing } })
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
            .alignment = composition::Alignment::bottom_trailing })
        .id(composition::ElementKey{ "shell.hints" });
}

auto ShellScreen::settingsPanel(RailEdge edge) const -> composition::Element
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
            .glass_panel(components::PanelRadius::large);
    return components::VStack(components::VStackOptions{ .spacing = theme::space::none }, std::move(panel))
        .padding(theme::Insets{ .top = theme::Space{ settingsTop },
            .leading = theme::Space{ settingsSide },
            .bottom = theme::Space{ settingsBottom + railBand(edge) },
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
        choices.push_back(Confirmation::Choice{
            .label = strings->text(shown.retry ? "akira/cloud/start_back" : "akira/stream/no"), .chosen = dismiss, .key = "ending.no" });
        choices.push_back(Confirmation::Choice{ .label = strings->text(shown.retry ? "akira/cloud/start_retry" : "akira/stream/yes"),
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

auto ShellScreen::dnsCard(composition::Element content, const ShellModel::View& view, composition::Namespace::ID focus) const
    -> composition::Element
{
    if (!view.dns)
        return content;
    ShellModel* held = model;
    const bool waiting = view.dns->wait > 0;
    std::vector<Confirmation::Choice> choices;
    choices.push_back(Confirmation::Choice{
        .label = waiting ? strings->format("akira/network/dns_ok_in", view.dns->wait) : strings->text("akira/common/ok"),
        .chosen = [held] { held->dismissDns(); },
        .emphasis = Confirmation::Emphasis::Prominent,
        .key = "dns.ok",
        .disabled = waiting });
    std::function<void()> dismissed;
    if (!waiting)
        dismissed = [held] { held->dismissDns(); };
    return std::move(content).overlay(true, composition::OverlayLayer::dialog,
        Confirmation(strings->text("akira/network/dns_failed_title"),
            strings->format("akira/network/dns_failed", view.dns->domain, view.dns->server), std::move(choices),
            std::move(dismissed))
            .focusing(focus)
            .dimmed("dns"));
}

auto ShellScreen::appletNotice() const -> composition::Element
{
    ShellModel* held = model;
    composition::Element card =
        components::VStack(components::VStackOptions{ .spacing = theme::Space{ 18.0F } },
            composition::Element(BrandLockup(noticeLogoHeight, noticeLogoHeight * noticeMascotScale, BrandLockup::Motion::Idle))
                .padding(theme::Edge::bottom, theme::Space{ 12.0F }),
            components::Text(strings->text("akira/app/applet_warning"))
                .font(theme::Font::title3)
                .font_weight(theme::Font::Weight::semibold)
                .multiline_text_alignment(composition::TextAlignment::center)
                .id(composition::ElementKey{ "applet.message" }),
            components::Button(strings->text("akira/app/exit"), [held] { held->confirmExit(); })
                .button_style(components::ButtonStyle::glass_prominent())
                .id(composition::ElementKey{ "applet.exit" }))
            .padding(theme::Insets::symmetric(theme::Space{ 48.0F }, theme::Space{ 40.0F }))
            .frame({ .width = noticeWidth })
            .glass_panel(components::PanelRadius::modal);
    return components::ZStack(
        composition::Element(AmbientBackground(AmbientBackground::Presence::Quiet))
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity }),
        std::move(card))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .background(theme::ColourRole::window_background)
        .on_exit_command(strings->text("akira/app/exit"), [held] { held->confirmExit(); })
        .id(composition::ElementKey{ "applet" });
}

}
