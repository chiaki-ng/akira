#include "consoles/ui/console_field_sheet.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/provider/text_input/text_input_traits.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>

namespace akira::consoles::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace provider = ::arisa::provider;
namespace theme = ::arisa::theme;

const composition::StateKey<std::string> ConsoleFieldSheet::text{ "console-field-text", std::string() };

ConsoleFieldSheet::ConsoleFieldSheet(Field field, std::string catalogName, std::string initial, ConsoleShelf& shelf,
    ConsoleCommands& commands, const app::Strings& strings)
    : field(field)
    , catalogName(std::move(catalogName))
    , initial(std::move(initial))
    , shelf(&shelf)
    , commands(&commands)
    , strings(&strings)
{
}

auto ConsoleFieldSheet::title() const -> std::string
{
    if (field == Field::ConsolePin)
        return strings->text("akira/host_settings/console_pin");
    return strings->text(initial.empty() ? "akira/hosts/add_address" : "akira/hosts/set_address");
}

auto ConsoleFieldSheet::body(composition::Context& context) const -> composition::Element
{
    const auto entered = context.state(text, initial);
    const bool pin = field == Field::ConsolePin;
    controls::TextFieldOptions options{ .prompt = strings->text(
                                            pin ? "akira/host_settings/console_pin_placeholder" : "akira/add_host/ip_placeholder") };
    return components::VStack(
        components::VStackOptions{ .alignment = components::HorizontalAlignment::leading, .spacing = theme::space::lg },
        components::Text(title()).font(theme::Font::title2),
        controls::TextField(title(), entered, options)
            .keyboard_type(pin ? provider::KeyboardType::number_pad : provider::KeyboardType::numbers_and_punctuation)
            .id(composition::ElementKey{ "console-field.input" }),
        components::HStack(components::HStackOptions{ .spacing = theme::space::md }, components::Spacer(),
            components::Button(strings->text("akira/common/cancel"), components::ButtonRole::cancel,
                [shelf = shelf] { shelf->dismiss(); })
                .button_style(components::ButtonStyle::glass(theme::Glass::clear())),
            components::Button(strings->text("akira/common/save"),
                [commands = commands, catalogName = catalogName, entered, pin] {
                    if (pin)
                        commands->setConsolePin(catalogName, entered.get());
                    else
                        commands->setAddress(catalogName, entered.get());
                })
                .button_style(components::ButtonStyle::glass_prominent())
                .id(composition::ElementKey{ "console-field.save" })))
        .padding(theme::Space{ 24.0F })
        .frame({ .min_width = 420.0F });
}

}
