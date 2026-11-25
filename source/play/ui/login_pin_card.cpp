#include "play/ui/login_pin_card.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/structure/pin_field.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>
#include <vector>

namespace akira::play::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

const composition::StateKey<std::string> LoginPinCard::entered{ "login-pin", std::string() };

LoginPinCard::LoginPinCard(bool retry, StreamCommands& commands, const app::Strings& strings)
    : retry(retry)
    , commands(&commands)
    , strings(&strings)
{
}

auto LoginPinCard::pin(const std::string& typed) -> std::optional<consoles::ConsolePin>
{
    if (typed.size() != consoles::ConsolePin::length)
        return std::nullopt;
    return consoles::ConsolePin::parse(typed);
}

auto LoginPinCard::body(composition::Context& context) const -> composition::Element
{
    const auto typed = context.state(entered);
    StreamCommands* held = commands;
    auto submit = [held, typed](const std::string& code) {
        std::optional<consoles::ConsolePin> chosen = pin(code);
        if (!chosen)
            return;
        typed.set(std::string());
        held->submitLoginPin(std::move(*chosen));
    };
    auto cancel = [held, typed] {
        typed.set(std::string());
        held->cancelLoginPin();
    };
    std::vector<composition::Element> rows;
    rows.push_back(components::Image(components::Image::SystemName{ "lock" })
                       .font(theme::Font::system(symbolSize, theme::Font::Weight::medium))
                       .frame({ .width = symbolSide, .height = symbolSide })
                       .glass_effect(theme::Glass::regular(), theme::Shape::circle()));
    rows.push_back(components::Text(strings->text("akira/pin/login_prompt"))
                       .font(theme::Font::title2)
                       .font_weight(theme::Font::Weight::semibold)
                       .id(composition::ElementKey{ "login-pin.title" }));
    rows.push_back(structure::PinField(consoles::ConsolePin::length,
        composition::Binding<std::string>([typed] { return typed.get(); }, [typed](std::string value) { typed.set(std::move(value)); }),
        structure::PinField::Options{ .secure = true, .complete = submit })
                       .id(composition::ElementKey{ "login-pin.field" }));
    if (retry)
        rows.push_back(components::Text(strings->text("akira/pin/login_error"))
                           .font(theme::Font::callout)
                           .foreground_style(theme::ColourRole::red)
                           .id(composition::ElementKey{ "login-pin.error" }));
    rows.push_back(components::HStack(components::HStackOptions{ .spacing = theme::space::md },
        components::Button(strings->text("akira/common/cancel"), components::ButtonRole::cancel, cancel)
            .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
            .frame({ .max_width = composition::infinity })
            .id(composition::ElementKey{ "login-pin.cancel" }),
        components::Button(strings->text("akira/add_host/continue"), [submit, typed] { submit(typed.get()); })
            .button_style(components::ButtonStyle::glass_prominent())
            .disabled(!pin(typed.get()))
            .frame({ .max_width = composition::infinity })
            .id(composition::ElementKey{ "login-pin.continue" })));
    return components::VStack(components::VStackOptions{ .spacing = theme::Space{ 14.0F } }, std::move(rows))
        .padding(theme::Space{ 24.0F })
        .frame({ .width = width })
        .glass_panel(components::PanelRadius::modal)
        .on_exit_command(strings->text("akira/common/cancel"), cancel)
        .id(composition::ElementKey{ "login-pin" });
}

}
