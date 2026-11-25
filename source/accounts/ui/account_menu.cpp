#include "accounts/ui/account_menu.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/button_border_shape.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/divider.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/controls/menu_style.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <format>
#include <string>
#include <utility>
#include <vector>

#include "accounts/ui/account_avatar.hpp"

namespace akira::accounts::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace theme = ::arisa::theme;

AccountMenu::AccountMenu(AccountModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto AccountMenu::choices(AccountModel& model, const AccountModel::View& view, const app::Strings& strings)
    -> std::function<composition::Element()>
{
    AccountModel* held = &model;
    std::vector<AccountDesk::Entry> entries = view.entries;
    const std::string unnamed = strings.text("akira/settings/unnamed_profile");
    const std::string add = strings.text("akira/settings/add_profile");
    return [held, entries = std::move(entries), unnamed, add] {
        std::vector<composition::Element> items;
        for (const AccountDesk::Entry& entry : entries)
        {
            const std::string label = entry.name.empty() ? unnamed : entry.name;
            auto chosen = [held, id = entry.id] { held->switchTo(id); };
            composition::Element item = entry.active
                ? composition::Element(components::Button(label, components::Image::SystemName{ "check" }, chosen))
                : composition::Element(components::Button(label, chosen));
            items.push_back(std::move(item).id(composition::ElementKey{ std::format("header.profile.{}", entry.id) }));
        }
        items.push_back(components::Divider());
        items.push_back(components::Button(add, components::Image::SystemName{ "person_add" }, [held] { held->welcome(true); }));
        return composition::Element(components::Group(std::move(items)));
    };
}

auto AccountMenu::body(composition::Context& context) const -> composition::Element
{
    const AccountModel::View& view = context.read(model->view());
    const AccountDesk::Summary shown = view.summary.value_or(AccountDesk::Summary{});
    const std::string name = shown.name.empty() ? strings->text("akira/settings/no_active_profile") : shown.name;
    return controls::Menu(choices(*model, view, *strings),
        components::HStack(components::HStackOptions{ .spacing = theme::Space{ 10.0F } },
            AccountAvatar(shown.name, shown.avatarUrl, avatarSide),
            components::Text(name).font(theme::Font::callout).font_weight(theme::Font::Weight::medium).line_limit(1)
                .frame({ .max_width = nameWidth, .alignment = composition::Alignment::leading })))
        .menu_style(controls::MenuStyle::button())
        .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
        .button_border_shape(components::ButtonBorderShape::capsule())
        .id(composition::ElementKey{ "header.account" });
}

}
