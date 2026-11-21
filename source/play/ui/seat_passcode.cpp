#include "play/ui/seat_passcode.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/structure/pin_field.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <cctype>
#include <utility>
#include <vector>

namespace akira::play::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

const composition::StateKey<std::string> SeatPasscode::entered{ "seat-passcode", std::string() };

SeatPasscode::SeatPasscode(Couch& couch, const app::Strings& strings)
    : couch(&couch)
    , strings(&strings)
{
}

auto SeatPasscode::complete(const std::string& passcode) -> bool
{
    return passcode.size() == length
        && std::ranges::all_of(passcode, [](char digit) { return std::isdigit(static_cast<unsigned char>(digit)) != 0; });
}

auto SeatPasscode::body(composition::Context& context) const -> composition::Element
{
    const Couch::View& view = context.read(couch->view());
    const Couch::Passcode asked = view.passcode.value_or(Couch::Passcode{});
    const auto typed = context.state(entered);
    Couch* held = couch;
    std::vector<composition::Element> rows;
    rows.push_back(components::Text(strings->format("akira/couch/player_n", asked.slot + 1))
                       .font(theme::Font::title2)
                       .font_weight(theme::Font::Weight::semibold));
    rows.push_back(components::Text(strings->text("akira/pin/couch_passcode_prompt")).font(theme::Font::body));
    if (asked.retry)
        rows.push_back(components::Text(strings->text("akira/pin/couch_passcode_error"))
                           .font(theme::Font::callout)
                           .foreground_style(theme::ColourRole::red)
                           .id(composition::ElementKey{ "seat-passcode.error" }));
    auto submit = [held, typed](const std::string& passcode) {
        if (!complete(passcode))
            return;
        typed.set(std::string());
        held->submitPasscode(passcode);
    };
    rows.push_back(structure::PinField(length,
        composition::Binding<std::string>([typed] { return typed.get(); }, [typed](std::string value) { typed.set(std::move(value)); }),
        structure::PinField::Options{ .secure = true, .complete = submit })
                       .id(composition::ElementKey{ "seat-passcode.field" }));
    rows.push_back(components::HStack(components::HStackOptions{ .spacing = theme::space::md }, components::Spacer(),
        components::Button(strings->text("akira/common/cancel"), components::ButtonRole::cancel,
            [held, typed] {
                typed.set(std::string());
                held->cancelPasscode();
            })
            .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
            .id(composition::ElementKey{ "seat-passcode.cancel" }),
        components::Button(strings->text("akira/couch/join"), [submit, typed] { submit(typed.get()); })
            .button_style(components::ButtonStyle::glass_prominent())
            .disabled(!complete(typed.get()))
            .id(composition::ElementKey{ "seat-passcode.join" })));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 14.0F } },
        std::move(rows))
        .padding(theme::Space{ 24.0F })
        .frame({ .width = width })
        .glass_panel(components::PanelRadius::large)
        .on_exit_command(strings->text("akira/common/cancel"), [held] { held->cancelPasscode(); })
        .id(composition::ElementKey{ "seat-passcode" });
}

}
