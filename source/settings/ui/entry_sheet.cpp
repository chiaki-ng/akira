#include "settings/ui/entry_sheet.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

namespace akira::settings::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace provider = ::arisa::provider;
namespace theme = ::arisa::theme;

const composition::StateKey<std::string> EntrySheet::text{ "settings-entry-text", std::string() };

EntrySheet::EntrySheet(SettingsModel::Entry entry, SettingsModel& model, const app::Strings& strings)
    : entry(entry)
    , model(&model)
    , strings(&strings)
{
}

auto EntrySheet::titleKey(SettingsModel::Entry entry) -> std::string
{
    switch (entry)
    {
        case SettingsModel::Entry::Dns: return "akira/settings/custom_dns_prompt";
        case SettingsModel::Entry::UpdateServer: return "akira/settings/simulate_update";
        case SettingsModel::Entry::Subnet:
        case SettingsModel::Entry::None: break;
    }
    return "akira/settings/discovery_subnets_prompt";
}

auto EntrySheet::placeholderKey(SettingsModel::Entry entry) -> std::string
{
    switch (entry)
    {
        case SettingsModel::Entry::Dns: return "akira/settings/custom_dns_placeholder";
        case SettingsModel::Entry::UpdateServer: return "akira/settings/dev_update_server_hint";
        case SettingsModel::Entry::Subnet:
        case SettingsModel::Entry::None: break;
    }
    return "akira/settings/discovery_subnets_placeholder";
}

auto EntrySheet::rejectionKey(SettingsModel::Entry entry) -> std::string
{
    return entry == SettingsModel::Entry::Dns ? "akira/settings/custom_dns_invalid" : "akira/settings/subnet_invalid";
}

auto EntrySheet::body(composition::Context& context) const -> composition::Element
{
    const auto entered = context.state(text, model->initial(entry));
    const std::string title = strings->text(titleKey(entry));
    controls::TextFieldOptions options{ .prompt = strings->text(placeholderKey(entry)) };
    return components::VStack(
        components::VStackOptions{ .alignment = components::HorizontalAlignment::leading, .spacing = theme::space::lg },
        components::Text(title).font(theme::Font::headline),
        controls::TextField(title, entered, options)
            .keyboard_type(entry == SettingsModel::Entry::UpdateServer ? provider::KeyboardType::url
                                                                        : provider::KeyboardType::numbers_and_punctuation)
            .id(composition::ElementKey{ "settings-entry.input" }),
        components::HStack(components::HStackOptions{ .spacing = theme::space::md }, components::Spacer(),
            components::Button(strings->text("akira/common/cancel"), components::ButtonRole::cancel,
                [model = model] { model->dismissEntry(); })
                .button_style(components::ButtonStyle::glass(theme::Glass::clear())),
            components::Button(strings->text("akira/common/save"), [model = model, entered] { model->submit(entered.get()); })
                .button_style(components::ButtonStyle::glass_prominent())
                .id(composition::ElementKey{ "settings-entry.save" })))
        .padding(theme::Space{ 24.0F })
        .frame({ .max_width = 520.0F });
}

}
