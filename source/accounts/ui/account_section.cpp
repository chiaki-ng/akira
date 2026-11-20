#include "accounts/ui/account_section.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_sizing.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/presentation/alert.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/provider/text_input/text_input_traits.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <cctype>
#include <format>
#include <utility>

namespace akira::accounts::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace provider = ::arisa::provider;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

AccountSection::AccountSection(AccountModel& model, const settings::HelpCatalog& help, const app::Strings& strings)
    : model(&model)
    , help(&help)
    , strings(&strings)
{
}

auto AccountSection::described(composition::Element row, std::string_view id) const -> composition::Element
{
    const std::string title = help->title(id);
    const std::string body = help->body(id);
    if (title.empty() && body.empty())
        return row;
    return std::move(row).help(title, body);
}

auto AccountSection::initial(const std::string& name) -> std::string
{
    if (name.empty())
        return "?";
    return std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(name.front()))));
}

auto AccountSection::kindKey(AccountDesk::Kind kind) -> const char*
{
    switch (kind)
    {
        case AccountDesk::Kind::Remote: return "akira/settings/profile_remote";
        case AccountDesk::Kind::Legacy: return "akira/settings/profile_legacy";
        case AccountDesk::Kind::Local: break;
    }
    return "akira/settings/profile_local";
}

auto AccountSection::avatar(const std::string& name, float side) -> composition::Element
{
    return components::Text(initial(name))
        .font(theme::Font::system(side * 0.42F, theme::Font::Weight::semibold))
        .foreground_style(theme::ColourRole::on_accent)
        .frame({ .width = side, .height = side })
        .background(theme::ColourRole::accent, theme::Shape::circle());
}

auto AccountSection::section(std::string header, std::vector<composition::Element> rows) -> composition::Element
{
    return structure::Section(structure::Section::Labels{ .header = components::Text(std::move(header)) }, std::move(rows));
}

auto AccountSection::card(const AccountModel::View& view) const -> composition::Element
{
    const AccountDesk::Summary shown = view.summary.value_or(AccountDesk::Summary{});
    const std::string name = shown.name.empty() ? strings->text("akira/settings/no_active_profile") : shown.name;
    std::string status = strings->text(kindKey(shown.kind));
    if (!shown.cloudKey.empty())
        status += "  ·  " + strings->text(shown.cloudKey);
    std::vector<composition::Element> lines;
    std::vector<composition::Element> title;
    title.push_back(components::Text(name).font(theme::Font::title3).font_weight(theme::Font::Weight::semibold).line_limit(1));
    if (shown.plus)
        title.push_back(components::Text(strings->text("akira/settings/ps_plus")).font(theme::Font::caption).foreground_style(theme::ColourRole::yellow));
    lines.push_back(components::HStack(components::HStackOptions{ .spacing = theme::space::sm }, std::move(title)));
    lines.push_back(components::Text(status).font(theme::Font::callout).foreground_style(theme::ColourRole::secondary_label));
    if (shown.trophies)
    {
        const AccountDesk::Trophies& earned = *shown.trophies;
        lines.push_back(components::Text(strings->format("akira/settings/trophy_level_line", earned.level, earned.progress))
                            .font(theme::Font::footnote)
                            .foreground_style(theme::ColourRole::secondary_label));
        lines.push_back(components::HStack(components::HStackOptions{ .spacing = theme::space::md },
            components::Label(std::format("{}", earned.platinum), components::Image::SystemName{ "trophy" })
                .foreground_style(theme::ColourRole::cyan),
            components::Label(std::format("{}", earned.gold), components::Image::SystemName{ "trophy" })
                .foreground_style(theme::ColourRole::yellow),
            components::Label(std::format("{}", earned.silver), components::Image::SystemName{ "trophy" })
                .foreground_style(theme::ColourRole::gray),
            components::Label(std::format("{}", earned.bronze), components::Image::SystemName{ "trophy" })
                .foreground_style(theme::ColourRole::brown))
                            .font(theme::Font::footnote)
                            .id(composition::ElementKey{ "account.trophies" }));
    }
    return components::HStack(components::HStackOptions{ .spacing = theme::Space{ 16.0F } }, avatar(shown.name, avatarSize),
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                               .spacing = theme::Space{ 4.0F } },
            std::move(lines)),
        components::Spacer())
        .id(composition::ElementKey{ "account.card" });
}

auto AccountSection::profiles(const AccountModel::View& view) const -> composition::Element
{
    AccountModel* held = model;
    std::vector<composition::Element> rows;
    rows.push_back(card(view));
    const bool only = view.entries.size() == 1;
    for (const AccountDesk::Entry& entry : view.entries)
    {
        const std::string name = entry.name.empty() ? strings->text("akira/settings/unnamed_profile") : entry.name;
        std::vector<composition::Element> label;
        label.push_back(avatar(entry.name, rowAvatarSize));
        label.push_back(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                               .spacing = theme::Space{ 1.0F } },
            components::Text(name).line_limit(1),
            components::Text(strings->text(kindKey(entry.kind))).font(theme::Font::caption).foreground_style(theme::ColourRole::secondary_label)));
        label.push_back(components::Spacer());
        if (entry.active)
            label.push_back(components::Image(components::Image::SystemName{ "check" }).foreground_style(theme::ColourRole::accent));
        std::vector<composition::Element> line;
        line.push_back(components::Button([held, id = entry.id] { held->switchTo(id); },
            components::HStack(components::HStackOptions{ .spacing = theme::Space{ 12.0F } }, std::move(label)))
                           .button_sizing(components::ButtonSizing::flexible)
                           .id(composition::ElementKey{ std::format("account.profile.{}", entry.id) }));
        if (!entry.active || only)
            line.push_back(components::Button([held, id = entry.id] { held->askRemove(id); },
                components::Image(components::Image::SystemName{ "delete" }).foreground_style(theme::ColourRole::secondary_label))
                               .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                               .id(composition::ElementKey{ std::format("account.remove.{}", entry.id) }));
        rows.push_back(components::HStack(components::HStackOptions{ .spacing = theme::space::sm }, std::move(line)));
    }
    rows.push_back(components::Button(strings->text("akira/settings/add_profile"), components::Image::SystemName{ "person_add" },
        [held] { held->welcome(true); })
                       .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                       .id(composition::ElementKey{ "account.add" }));
    return section(strings->text("akira/settings/profiles"), std::move(rows));
}

auto AccountSection::identity(const AccountModel::View& view) const -> composition::Element
{
    AccountModel* held = model;
    std::vector<composition::Element> rows;
    rows.push_back(described(components::LabeledContent(strings->text("akira/settings/online_id"), controls::TextField(strings->text("akira/settings/online_id"),
                                 composition::Binding<std::string>([online = view.identity.onlineId] { return online; },
                                     [held](std::string text) { held->setOnlineId(text); }))
                                 .labels_hidden()
                                 .id(composition::ElementKey{ "account.online-id" })),
        "settings/psnOnlineId"));
    rows.push_back(described(components::LabeledContent(strings->text("akira/settings/psn_account_id"), controls::TextField(strings->text("akira/settings/psn_account_id"),
                                 composition::Binding<std::string>([account = view.identity.accountId] { return account; },
                                     [held](std::string text) { held->setAccountId(text); }),
                                 controls::TextFieldOptions{ .prompt = strings->text("akira/settings/psn_account_id_placeholder") })
                                 .labels_hidden()
                                 .id(composition::ElementKey{ "account.account-id" })),
        "settings/psnAccountId"));
    if (!view.identity.legacy)
        rows.push_back(described(controls::Toggle(strings->text("akira/settings/trophies_enabled"),
                                     composition::Binding<bool>([trophies = view.identity.trophies] { return trophies; },
                                         [held](bool enabled) { held->setTrophies(enabled); }))
                                     .id(composition::ElementKey{ "account.trophies-enabled" }),
            "settings/trophiesEnabled"));
    return section(strings->text("akira/settings/profile_heading"), std::move(rows));
}

auto AccountSection::companion(const AccountModel::View& view) const -> composition::Element
{
    AccountModel* held = model;
    std::vector<composition::Element> rows;
    rows.push_back(described(components::LabeledContent(strings->text("akira/settings/companion_port"), controls::TextField(strings->text("akira/settings/companion_port"),
                                 composition::Binding<std::string>([port = std::format("{}", view.identity.companionPort)] { return port; },
                                     [held](std::string text) { held->setCompanionPort(text); }),
                                 controls::TextFieldOptions{ .prompt = strings->text("akira/settings/companion_port_placeholder") })
                                 .keyboard_type(provider::KeyboardType::number_pad)
                                 .labels_hidden()
                                 .id(composition::ElementKey{ "account.companion-port" })),
        "settings/companionPort"));
    rows.push_back(described(components::Button(strings->text("akira/settings/pair_companion"),
                                 components::Image::SystemName{ "computer" }, [held] { held->startCompanion(); })
                                 .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                                 .id(composition::ElementKey{ "account.pair" }),
        "settings/pairBtn"));
    return section(strings->text("akira/settings/companion_heading"), std::move(rows));
}

auto AccountSection::credential(const AccountDesk::Credential& shown) const -> composition::Element
{
    std::string text = shown.shown.value_or(strings->text("akira/common/not_set"));
    if (shown.expired)
        text += strings->text("akira/settings/expired_suffix");
    return components::LabeledContent(strings->text(AccountModel::secretKey(shown.secret)), std::move(text))
        .id(composition::ElementKey{ std::format("account.secret.{}", static_cast<int>(shown.secret)) });
}

auto AccountSection::refreshLabel(const AccountDesk::Refresh& refresh) const -> std::string
{
    switch (refresh.state)
    {
        case AccountDesk::RefreshState::Busy: return strings->text("akira/settings/refresh_token_busy");
        case AccountDesk::RefreshState::CoolingDown: return strings->format("akira/settings/refresh_token_wait", refresh.seconds);
        case AccountDesk::RefreshState::Ready:
        case AccountDesk::RefreshState::NotLinked: break;
    }
    return strings->text("akira/settings/refresh_token_btn");
}

auto AccountSection::tokens(const AccountModel::View& view) const -> composition::Element
{
    AccountModel* held = model;
    std::vector<composition::Element> rows;
    for (const AccountDesk::Credential& shown : view.credentials)
        rows.push_back(credential(shown));
    const bool refreshable = view.refresh.state == AccountDesk::RefreshState::Ready
        || view.refresh.state == AccountDesk::RefreshState::NotLinked;
    rows.push_back(components::HStack(components::HStackOptions{ .spacing = theme::space::sm },
        components::Button(strings->text(view.revealed ? "akira/settings/hide_secrets" : "akira/settings/reveal_secrets"),
            components::Image::SystemName{ view.revealed ? "visibility_off" : "visibility" },
            [held, revealed = view.revealed] { held->reveal(!revealed); })
            .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
            .help(help->title("settings/revealCredentials"), help->body("settings/revealCredentials"))
            .id(composition::ElementKey{ "account.reveal" }),
        components::Button(refreshLabel(view.refresh), components::Image::SystemName{ "refresh" }, [held] { held->refreshToken(); })
            .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
            .disabled(!refreshable)
            .help(help->title("settings/refreshTokenBtn"), help->body("settings/refreshTokenBtn"))
            .id(composition::ElementKey{ "account.refresh" }),
        components::Button(strings->text("akira/settings/clear_psn_data"), components::Image::SystemName{ "delete" },
            components::ButtonRole::destructive, [held] { held->askClear(); })
            .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
            .help(help->title("settings/clearPsnBtn"), help->body("settings/clearPsnBtn"))
            .id(composition::ElementKey{ "account.clear" })));
    return section(strings->text("akira/settings/psn_tokens_heading"), std::move(rows));
}

auto AccountSection::confirmations(composition::Element screen, const AccountModel::View& view) const -> composition::Element
{
    AccountModel* held = model;
    std::string removed;
    if (view.removing)
    {
        const auto found = std::ranges::find(view.entries, *view.removing, &AccountDesk::Entry::id);
        removed = found == view.entries.end() || found->name.empty() ? strings->text("akira/settings/unnamed_profile") : found->name;
    }
    const std::string removal = view.entries.size() <= 1 ? strings->text(AccountModel::removalKey(view.entries.size()))
                                                         : strings->format(AccountModel::removalKey(view.entries.size()), removed);
    composition::Binding<bool> removing([held] { return held->view().get().removing.has_value(); },
        [held](bool open) {
            if (!open)
                held->dismissRemove();
        });
    composition::Binding<bool> clearing([held] { return held->view().get().clearing; },
        [held](bool open) {
            if (!open)
                held->dismissClear();
        });
    return std::move(screen)
        .alert(strings->text("akira/settings/remove_profile"), std::move(removing),
            [held, cancel = strings->text("akira/common/cancel"), remove = strings->text("akira/settings/remove_profile")] {
                return components::Group(components::Button(cancel, components::ButtonRole::cancel, [held] { held->dismissRemove(); }),
                    components::Button(remove, components::ButtonRole::destructive, [held] { held->confirmRemove(); }));
            },
            [removal] { return components::Text(removal); })
        .alert(strings->text("akira/settings/clear_psn_data"), std::move(clearing),
            [held, cancel = strings->text("akira/common/cancel"), clear = strings->text("akira/settings/clear_all")] {
                return components::Group(components::Button(cancel, components::ButtonRole::cancel, [held] { held->dismissClear(); }),
                    components::Button(clear, components::ButtonRole::destructive, [held] { held->confirmClear(); }));
            },
            [message = strings->text("akira/settings/clear_psn_confirm")] { return components::Text(message); });
}

auto AccountSection::body(composition::Context& context) const -> composition::Element
{
    const AccountModel::View& view = context.read(model->view());
    AccountModel* held = model;
    std::vector<composition::Element> sections;
    sections.push_back(profiles(view));
    if (!view.entries.empty())
    {
        sections.push_back(identity(view));
        sections.push_back(companion(view));
        if (!view.identity.legacy)
            sections.push_back(tokens(view));
    }
    return confirmations(composition::Element(structure::Form(std::move(sections)))
                             .on_appear([held] { held->reload(); })
                             .id(composition::ElementKey{ "account" }),
        view);
}

}
