#include "settings/ui/settings_screen.hpp"

#include <arisa/components/control_size.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_border_shape.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/divider.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/controls/toggle_style.hpp>
#include <arisa/presentation/alert.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/dock_bar.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/form_style.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>

#include "accounts/ui/account_section.hpp"
#include "app/arisa/about_page.hpp"
#include "app/arisa/build_info_page.hpp"
#include "app/version.hpp"
#include "log/ui/log_browser.hpp"
#include "network/ui/nat_section.hpp"
#include "network/ui/tunnel_section.hpp"
#include "play/ui/button_board_view.hpp"
#include "settings/developer_settings.hpp"
#include "settings/ui/config_page.hpp"
#include "settings/ui/benchmark_page.hpp"
#include "settings/ui/controls_section.hpp"
#include "settings/ui/debug_section.hpp"
#include "settings/ui/developer_section.hpp"
#include "settings/ui/discovery_log_page.hpp"
#include "settings/ui/entry_sheet.hpp"
#include "settings/ui/general_section.hpp"
#include "settings/ui/pad_page.hpp"
#include "settings/ui/picture_section.hpp"
#include "settings/ui/power_user_section.hpp"
#include "settings/ui/updates_section.hpp"

namespace akira::settings::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace provider = ::arisa::provider;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

SettingsScreen::SettingsScreen(Parts parts, const app::Strings& strings)
    : parts(parts)
    , strings(&strings)
{
}

auto SettingsScreen::pages(const SettingsModel& model) -> std::vector<Page>
{
    std::vector<Page> shown;
    if (model.features().accounts)
        shown.push_back(Page::Account);
    shown.insert(shown.end(), { Page::General, Page::Picture, Page::Controls });
    if (model.features().selfUpdate)
        shown.push_back(Page::Updates);
    shown.push_back(Page::Network);
    shown.push_back(Page::Debug);
    shown.push_back(Page::Config);
    if (model.parts().developer.powerUserMenuUnlocked())
    {
        shown.push_back(Page::PowerUser);
        shown.push_back(Page::Developer);
    }
    shown.push_back(Page::BuildInfo);
    shown.push_back(Page::About);
    return shown;
}

auto SettingsScreen::next(const std::vector<Page>& shown, Page current, int step) -> Page
{
    const auto found = std::ranges::find(shown, current);
    const int count = static_cast<int>(shown.size());
    const int at = found == shown.end() ? 0 : static_cast<int>(found - shown.begin());
    return shown[static_cast<std::size_t>(((at + step) % count + count) % count)];
}

auto SettingsScreen::title(Page shown) const -> std::string
{
    switch (shown)
    {
        case Page::Account: return strings->text("akira/settings/menu_account");
        case Page::BuildInfo: return strings->text("akira/settings/menu_build_info");
        case Page::About: return strings->text("akira/settings/menu_about");
        case Page::General: return strings->text("akira/settings/menu_general");
        case Page::Picture: return strings->text("akira/settings/menu_picture");
        case Page::Controls: return strings->text("akira/settings/menu_controls");
        case Page::Updates: return strings->text("akira/settings/menu_updates");
        case Page::Network: return strings->text("akira/settings/menu_network");
        case Page::Debug: return strings->text("akira/settings/menu_debug");
        case Page::Config: return strings->text("akira/settings/menu_config");
        case Page::PowerUser: return strings->text("akira/settings/menu_power_user");
        case Page::Developer: return strings->text("akira/settings/menu_developer");
    }
    return {};
}

auto SettingsScreen::symbol(Page shown) -> std::string
{
    switch (shown)
    {
        case Page::Account: return "account_circle";
        case Page::BuildInfo: return "memory";
        case Page::About: return "info";
        case Page::General: return "settings";
        case Page::Picture: return "tv";
        case Page::Controls: return "sports_esports";
        case Page::Updates: return "update";
        case Page::Network: return "wifi";
        case Page::Debug: return "report";
        case Page::Config: return "description";
        case Page::PowerUser: return "bolt";
        case Page::Developer: return "science";
    }
    return "circle";
}

auto SettingsScreen::content(Page shown) const -> composition::Element
{
    SettingsModel& model = *parts.model;
    const HelpCatalog& help = *parts.help;
    switch (shown)
    {
        case Page::Account:
            if (parts.accounts)
                return accounts::ui::AccountSection(*parts.accounts, help, *strings);
            break;
        case Page::BuildInfo: return app::arisa::BuildInfoPage(model.features().buildInfoFile, *strings);
        case Page::About: return app::arisa::AboutPage(*strings);
        case Page::General: return GeneralSection(model, help, *strings);
        case Page::Picture: break;
        case Page::Controls: return ControlsSection(model, help, *strings);
        case Page::Updates: return UpdatesSection(model, help, *strings);
        case Page::Network:
            return structure::Form(network::ui::NatSection(*parts.nat, help, *strings),
                network::ui::TunnelSection(*parts.tunnel, *strings))
                .on_appear([tunnel = parts.tunnel] { tunnel->refresh(); });
        case Page::Debug: return DebugSection(model, help, *strings);
        case Page::Config: return ConfigPage(model, *strings);
        case Page::PowerUser: return PowerUserSection(model, help, *strings);
        case Page::Developer: return DeveloperSection(model, help, *strings);
    }
    return PictureSection(*parts.picture, model, help, *strings);
}

auto SettingsScreen::detail(const SettingsModel::Status& status) const -> composition::Element
{
    SettingsModel* model = parts.model;
    std::string heading;
    composition::Element shown = components::Group();
    float width = contentWidth;
    switch (status.detail)
    {
        case SettingsModel::Detail::Mapping:
            heading = strings->text("akira/remap/title");
            shown = components::ScrollView(play::ui::ButtonBoardView(*parts.mapping, *strings, true));
            width = boardWidth;
            break;
        case SettingsModel::Detail::Pad:
            heading = status.pad < status.pads.size() ? status.pads[status.pad].label : std::string();
            shown = PadPage(*model, *parts.help, *strings);
            break;
        case SettingsModel::Detail::Logs:
            heading = strings->text("akira/settings/dev_logs");
            if (parts.logs)
                shown = composition::Element(log::ui::LogBrowser(*parts.logs, *strings))
                            .on_appear([logs = parts.logs] { logs->open(); });
            break;
        case SettingsModel::Detail::DiscoveryLog:
            heading = strings->text("akira/settings/discovery_log_viewer");
            shown = DiscoveryLogPage(*model, *strings);
            break;
        case SettingsModel::Detail::Benchmark:
            heading = strings->text("akira/benchmark/title");
            shown = BenchmarkPage(*model, *strings);
            break;
        case SettingsModel::Detail::None: break;
    }
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 12.0F } },
        components::HStack(components::HStackOptions{ .spacing = theme::Space{ 10.0F } },
            components::Button([model] { model->close(); },
                components::Image(components::Image::SystemName{ "arrow_back" }).frame({ .width = 20.0F, .height = 20.0F }))
                .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                .button_border_shape(components::ButtonBorderShape::circle())
                .id(composition::ElementKey{ "settings.detail.back" }),
            components::Text(heading).font(theme::Font::title3).font_weight(theme::Font::Weight::semibold)),
        std::move(shown)
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
                .alignment = composition::Alignment::top_leading }))
        .frame({ .width = width, .max_height = composition::infinity })
        .on_exit_command(strings->text("akira/common/back"), [model] { model->close(); })
        .id(composition::ElementKey{ "settings.detail" });
}

auto SettingsScreen::presented(composition::Element screen, const SettingsModel::Status& status) const
    -> composition::Element
{
    SettingsModel* model = parts.model;
    const SettingsModel::Entry asked = status.entry;
    composition::Binding<bool> entering([model] { return model->view().get().entry != SettingsModel::Entry::None; },
        [model](bool open) {
            if (!open)
                model->dismissEntry();
        });
    composition::Binding<bool> rejected([model] { return model->view().get().rejected != SettingsModel::Entry::None; },
        [model](bool open) {
            if (!open)
                model->dismissRejection();
        });
    composition::Binding<bool> offered([model] { return model->view().get().offered; },
        [model](bool open) {
            if (!open)
                model->dismissOffer();
        });
    composition::Binding<bool> installing([model] { return model->view().get().install.has_value(); },
        [model](bool open) {
            if (!open)
                model->dismissInstall();
        });
    const std::string version = status.update ? status.update->version : std::string();
    std::string progress;
    if (status.install)
    {
        switch (status.install->stage)
        {
            case Maintenance::UpdateProgress::Stage::Downloading:
                progress = strings->format("akira/update/downloading", status.install->percent);
                break;
            case Maintenance::UpdateProgress::Stage::Verifying: progress = strings->text("akira/update/verifying"); break;
            case Maintenance::UpdateProgress::Stage::Installed: progress = strings->text("akira/update/installed_relaunch"); break;
            case Maintenance::UpdateProgress::Stage::Failed: progress = status.install->error; break;
        }
    }
    const bool settled = status.install
        && (status.install->stage == Maintenance::UpdateProgress::Stage::Installed
            || status.install->stage == Maintenance::UpdateProgress::Stage::Failed);
    const std::string rejection = strings->text(EntrySheet::rejectionKey(status.rejected));
    return std::move(screen)
        .sheet(std::move(entering),
            [model, asked, words = strings] {
                return composition::Element(EntrySheet(asked, *model, *words))
                    .presentation_sizing(composition::PresentationSizing::fitted);
            })
        .alert(rejection, std::move(rejected),
            [model, ok = strings->text("akira/common/ok")] {
                return components::Button(ok, components::ButtonRole::cancel, [model] { model->dismissRejection(); });
            })
        .alert(strings->text("akira/update/prompt_title"), std::move(offered),
            [model, now = strings->text("akira/update/update_now"), later = strings->text("akira/update/later")] {
                return components::Group(components::Button(later, components::ButtonRole::cancel, [model] { model->dismissOffer(); }),
                    components::Button(now, [model] { model->installUpdate(); }));
            },
            [body = strings->format("akira/update/prompt_body", version, app::version::semver())] { return components::Text(body); })
        .alert(progress, std::move(installing),
            [model, settled, ok = strings->text("akira/common/ok"), cancel = strings->text("akira/common/cancel")] {
                return components::Button(settled ? ok : cancel, components::ButtonRole::cancel, [model] { model->dismissInstall(); });
            });
}

auto SettingsScreen::body(composition::Context& context) const -> composition::Element
{
    const SettingsModel::Status& status = context.read(parts.model->view());
    const std::vector<Page> shown = pages(*parts.model);
    const auto chosen = context.state(page);
    const Page current = std::ranges::contains(shown, chosen.get()) ? chosen.get() : Page::About;
    composition::Binding<std::size_t> selection(
        [chosen, shown] {
            const auto found = std::ranges::find(shown, chosen.get());
            return found == shown.end() ? std::size_t{ 0 } : static_cast<std::size_t>(found - shown.begin());
        },
        [chosen, shown, model = parts.model](std::size_t index) {
            if (index < shown.size())
            {
                model->close();
                chosen.set(shown[index]);
            }
        });
    std::vector<structure::DockBar::Item> tabs;
    for (const Page tab : shown)
        tabs.push_back({ title(tab), symbol(tab) });
    composition::Element body = status.detail == SettingsModel::Detail::None ? content(current).frame({ .width = contentWidth, .max_height = composition::infinity })
                                                                               : detail(status);
    SettingsModel* model = parts.model;
    composition::Element screen =
        components::VStack(components::VStackOptions{ .spacing = theme::space::none },
            composition::Element(structure::DockBar(std::move(selection), std::move(tabs), {},
                                     structure::DockBar::Options{ .follows_focus = true }))
                .control_size(components::ControlSize::small)
                .labels_hidden()
                .padding(theme::Edge::top, theme::Space{ 8.0F })
                .id(composition::ElementKey{ "settings.pages" }),
            composition::Element(components::Group()).frame({ .height = 14.0F }),
            components::Divider(),
            std::move(body)
                .form_style(structure::FormStyle::cards())
                .toggle_style(controls::ToggleStyle::checkbox())
                .scroll_content_background(composition::Visibility::hidden)
                .padding(theme::Edge::top, theme::Space{ 6.0F })
                .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
                    .alignment = composition::Alignment::top }))
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
                .alignment = composition::Alignment::top })
            .on_command(provider::Button::l, strings->text("akira/settings/previous_page"),
                [chosen, shown, current, model] {
                    model->close();
                    chosen.set(next(shown, current, -1));
                })
            .on_command(provider::Button::r, strings->text("akira/settings/next_page"),
                [chosen, shown, current, model] {
                    model->close();
                    chosen.set(next(shown, current, 1));
                });
    return presented(std::move(screen), status);
}

}
