#include "play/ui/couch_prompt.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

namespace akira::play::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

CouchPrompt::CouchPrompt(Couch& couch, const app::Strings& strings)
    : couch(&couch)
    , strings(&strings)
{
}

auto CouchPrompt::body(composition::Context& context) const -> composition::Element
{
    static_cast<void>(context.read(couch->view()));
    Couch* held = couch;
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 18.0F } },
        components::HStack(components::HStackOptions{ .spacing = theme::Space{ 12.0F } },
            components::Image(components::Image::SystemName{ "sports_esports" })
                .font(theme::Font::title2)
                .foreground_style(theme::ColourRole::accent),
            components::Text(strings->text("akira/couch/controller_arrived")).font(theme::Font::headline)),
        components::HStack(components::HStackOptions{ .spacing = theme::space::md }, components::Spacer(),
            components::Button(strings->text("akira/couch/not_now"), components::ButtonRole::cancel,
                [held] { held->answerArrival(Couch::Arrival::NotNow); })
                .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                .id(composition::ElementKey{ "couch-prompt.not-now" }),
            components::Button(strings->text("akira/couch/add_player"), components::Image::SystemName{ "person_add" },
                [held] { held->answerArrival(Couch::Arrival::AddPlayer); })
                .button_style(components::ButtonStyle::glass_prominent())
                .id(composition::ElementKey{ "couch-prompt.add" })))
        .padding(theme::Space{ 24.0F })
        .frame({ .width = width })
        .glass_panel(components::PanelRadius::large)
        .on_exit_command(strings->text("akira/common/back"), [held] { held->answerArrival(Couch::Arrival::Dismissed); })
        .id(composition::ElementKey{ "couch-prompt" });
}

}
