#include "consoles/ui/pin_pad.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/pin_field.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>

namespace akira::consoles::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace provider = ::arisa::provider;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

const composition::StateKey<std::string> PinPad::digits{ "pin-pad-digits", std::string() };

PinPad::PinPad(std::string catalogName, std::string name, bool ps5, ConsoleCommands& commands,
    const app::Strings& strings)
    : catalogName(std::move(catalogName))
    , name(std::move(name))
    , ps5(ps5)
    , commands(&commands)
    , strings(&strings)
{
}

auto PinPad::body(composition::Context& context) const -> composition::Element
{
    const auto entered = context.state(digits);
    const bool complete = entered.get().size() == length;
    composition::Element pair = components::Button(strings->text("akira/pin/pair"),
        [commands = commands, catalogName = catalogName, entered] {
            if (entered.get().size() == length)
                commands->registerWithPin(catalogName, entered.get());
        })
                                    .button_style(components::ButtonStyle::glass_prominent())
                                    .disabled(!complete)
                                    .id(composition::ElementKey{ "pin.pair" });
    composition::Element form = components::VStack(
        components::VStackOptions{ .alignment = components::HorizontalAlignment::leading, .spacing = theme::space::md },
        components::Text(strings->format("akira/pin/pair_with", name))
            .font(theme::Font::title2)
            .font_weight(theme::Font::Weight::semibold),
        components::Text(strings->text("akira/pin/registration_prompt"))
            .font(theme::Font::callout)
            .foreground_style(theme::ColourRole::secondary_label),
        structure::PinField(length, composition::Binding<std::string>([entered] { return entered.get(); },
                                        [entered](std::string value) { entered.set(std::move(value)); }))
            .id(composition::ElementKey{ "pin.field" }),
        components::HStack(components::HStackOptions{ .spacing = theme::space::md }, components::Spacer(),
            components::Button(strings->text("akira/common/cancel"), components::ButtonRole::cancel,
                [commands = commands, catalogName = catalogName] { commands->cancelRegistration(catalogName); })
                .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                .id(composition::ElementKey{ "pin.cancel" }),
            std::move(pair)));
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::top,
                                  .spacing = theme::Space{ 22.0F } },
        components::Image(ps5 ? "img/console/ps5.png" : "img/console/ps4.png")
            .resizable()
            .aspect_ratio(std::nullopt, composition::ContentMode::fit)
            .frame({ .width = 92.0F, .height = 138.0F }),
        std::move(form))
        .padding(theme::Space{ 24.0F })
        .on_command(provider::Button::y, strings->text("akira/common/delete"),
            [entered] { entered.set(structure::PinField::erased(entered.get())); })
        .id(composition::ElementKey{ "pin.pad" });
}

}
