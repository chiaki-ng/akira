#include "consoles/ui/add_console_sheet.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/provider/text_input/text_input_traits.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>
#include <vector>

namespace akira::consoles::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace provider = ::arisa::provider;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

const composition::StateKey<std::string> AddConsoleSheet::name{ "add-console-name", std::string() };
const composition::StateKey<std::string> AddConsoleSheet::address{ "add-console-address", std::string() };

AddConsoleSheet::AddConsoleSheet(ConsoleShelf& shelf, ConsoleCommands& commands, const app::Strings& strings)
    : shelf(&shelf)
    , commands(&commands)
    , strings(&strings)
{
}

auto AddConsoleSheet::body(composition::Context& context) const -> composition::Element
{
    const auto chosenName = context.state(name);
    const auto chosenAddress = context.state(address);
    const auto chosenPs5 = context.state(ps5);
    std::vector<composition::Element> leading;
    if (commands->findsRemote())
        leading.push_back(components::Button(strings->text("akira/hosts/find_remote"),
            components::Image::SystemName{ "public" }, [commands = commands] { commands->findRemote(); })
                              .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                              .id(composition::ElementKey{ "add-console.find-remote" }));
    leading.push_back(components::Spacer());
    leading.push_back(components::Button(strings->text("akira/common/cancel"), components::ButtonRole::cancel,
        [shelf = shelf] { shelf->openAdd(false); })
                          .button_style(components::ButtonStyle::glass(theme::Glass::clear())));
    leading.push_back(components::Button(strings->text("akira/add_host/add_console"),
        [commands = commands, chosenName, chosenAddress, chosenPs5] {
            commands->addManual(chosenName.get(), chosenAddress.get(), chosenPs5.get());
        })
                          .button_style(components::ButtonStyle::glass_prominent())
                          .id(composition::ElementKey{ "add-console.save" }));
    const std::string ps5Label = strings->text("akira/common/ps5");
    const std::string ps4Label = strings->text("akira/common/ps4");
    return components::VStack(
        components::VStackOptions{ .alignment = components::HorizontalAlignment::leading, .spacing = theme::space::lg },
        components::Text(strings->text("akira/add_host/title")).font(theme::Font::title2),
        structure::Form(structure::Section(
            controls::TextField(strings->text("akira/add_host/console_name"), chosenName,
                controls::TextFieldOptions{ .prompt = strings->text("akira/add_host/console_name_placeholder") })
                .id(composition::ElementKey{ "add-console.name" }),
            controls::TextField(strings->text("akira/add_host/ip_address"), chosenAddress,
                controls::TextFieldOptions{ .prompt = strings->text("akira/add_host/ip_placeholder") })
                .keyboard_type(provider::KeyboardType::numbers_and_punctuation)
                .id(composition::ElementKey{ "add-console.address" }),
            controls::Picker(strings->text("akira/add_host/console_type"), composition::Binding<bool>(chosenPs5),
                [ps5Label, ps4Label] {
                    return composition::Element(components::Group(components::Text(ps5Label).tag(true),
                        components::Text(ps4Label).tag(false)));
                })
                .picker_style(controls::PickerStyle::segmented()))),
        components::HStack(components::HStackOptions{ .spacing = theme::space::md }, std::move(leading)))
        .padding(theme::Space{ 24.0F })
        .frame({ .min_width = 520.0F });
}

}
