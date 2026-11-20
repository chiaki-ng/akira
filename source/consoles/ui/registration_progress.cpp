#include "consoles/ui/registration_progress.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>

namespace akira::consoles::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace theme = ::arisa::theme;

RegistrationProgress::RegistrationProgress(ConsoleShelf::Registering progress, ConsoleCommands& commands,
    const app::Strings& strings)
    : progress(std::move(progress))
    , commands(&commands)
    , strings(&strings)
{
}

auto RegistrationProgress::body(composition::Context&) const -> composition::Element
{
    auto cancel = [commands = commands, name = progress.catalogName] { commands->cancelRegistration(name); };
    return components::VStack(components::VStackOptions{ .spacing = theme::space::lg }, controls::ProgressView(),
        components::Text(strings->text("akira/hosts/registering_psn")).font(theme::Font::title3),
        components::Text(progress.stage).font(theme::Font::callout).foreground_style(theme::ColourRole::secondary_label),
        components::Button(strings->text("akira/common/cancel"), components::ButtonRole::cancel, cancel)
            .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
            .id(composition::ElementKey{ "registration.cancel" }))
        .padding(theme::Insets::symmetric(theme::Space{ 56.0F }, theme::Space{ 42.0F }))
        .glass_effect(theme::Glass::regular(), theme::Shape::rounded(28.0F))
        .on_exit_command(strings->text("akira/common/cancel"), cancel);
}

}
