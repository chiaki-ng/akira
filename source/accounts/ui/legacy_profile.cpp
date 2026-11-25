#include "accounts/ui/legacy_profile.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/form_style.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>
#include <vector>

namespace akira::accounts::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

const composition::StateKey<std::string> LegacyProfile::accountId{ "legacy-profile-account", std::string() };
const composition::StateKey<std::string> LegacyProfile::name{ "legacy-profile-name", std::string() };

LegacyProfile::LegacyProfile(AccountModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto LegacyProfile::body(composition::Context& context) const -> composition::Element
{
    const AccountModel::View& view = context.read(model->view());
    const auto typedAccount = context.state(accountId);
    const auto typedName = context.state(name);
    AccountModel* held = model;
    std::vector<composition::Element> rows;
    rows.push_back(components::Text(strings->text("akira/profile_setup/legacy_title"))
                       .font(theme::Font::title)
                       .font_weight(theme::Font::Weight::semibold));
    rows.push_back(structure::Form(structure::Section(
        components::LabeledContent(strings->text("akira/profile_setup/account_id"),
            controls::TextField(strings->text("akira/profile_setup/account_id"), typedAccount,
                controls::TextFieldOptions{ .prompt = strings->text("akira/profile_setup/account_id_placeholder") })
                .labels_hidden()
                .id(composition::ElementKey{ "legacy.account" })),
        components::LabeledContent(strings->text("akira/profile_setup/name"),
            controls::TextField(strings->text("akira/profile_setup/name"), typedName,
                controls::TextFieldOptions{ .prompt = strings->text("akira/profile_setup/name") })
                .labels_hidden()
                .id(composition::ElementKey{ "legacy.name" }))))
                       .form_style(structure::FormStyle::cards())
                       .scroll_content_background(composition::Visibility::hidden));
    if (view.legacyRejected)
        rows.push_back(components::Text(strings->text("akira/pair/status_bad_payload"))
                           .font(theme::Font::callout)
                           .foreground_style(theme::ColourRole::red)
                           .id(composition::ElementKey{ "legacy.error" }));
    rows.push_back(components::HStack(components::HStackOptions{ .spacing = theme::space::md }, components::Spacer(),
        components::Button(strings->text("akira/common/back"), components::ButtonRole::cancel, [held] { held->back(); })
            .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
            .id(composition::ElementKey{ "legacy.back" }),
        components::Button(strings->text("akira/profile_setup/create"),
            [held, typedAccount, typedName] { held->createLegacy(typedAccount.get(), typedName.get()); })
            .button_style(components::ButtonStyle::glass_prominent())
            .disabled(typedAccount.get().empty() || typedName.get().empty())
            .id(composition::ElementKey{ "legacy.create" })));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 18.0F } },
        std::move(rows))
        .padding(theme::Space{ 28.0F })
        .frame({ .width = width })
        .glass_panel(components::PanelRadius::large)
        .on_exit_command(strings->text("akira/common/back"), [held] { held->back(); })
        .id(composition::ElementKey{ "legacy" });
}

}
