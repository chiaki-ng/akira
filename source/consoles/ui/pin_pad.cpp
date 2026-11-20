#include "consoles/ui/pin_pad.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>
#include <vector>

namespace akira::consoles::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace provider = ::arisa::provider;
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
        components::Text(strings->format("akira/pin/pair_with", name)).font(theme::Font::title2),
        components::Text(strings->text("akira/pin/registration_prompt"))
            .font(theme::Font::subheadline)
            .foreground_style(theme::ColourRole::secondary_label),
        boxes(entered.get()), keys(entered),
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
            [entered] {
                if (!entered.get().empty())
                    entered.update([](std::string& value) { value.pop_back(); });
            })
        .id(composition::ElementKey{ "pin.pad" });
}

auto PinPad::boxes(const std::string& entered) -> composition::Element
{
    std::vector<composition::Element> shown;
    for (std::size_t index = 0; index < length; ++index)
    {
        const bool current = index == entered.size();
        const std::string digit = index < entered.size() ? std::string(1, entered[index]) : std::string();
        shown.push_back(components::Text(digit)
                            .font(theme::Font::title2)
                            .frame({ .width = 40.0F, .height = 48.0F })
                            .background(theme::ColourRole::quaternary_system_fill, theme::Shape::rounded(10.0F))
                            .border(current ? theme::ColourValue(theme::ColourRole::accent)
                                            : theme::ColourValue(theme::ColourRole::separator),
                                current ? theme::Stroke{ 2.0F } : theme::Stroke::hairline()));
    }
    return components::HStack(components::HStackOptions{ .spacing = theme::space::sm }, std::move(shown));
}

auto PinPad::keys(composition::StateHandle<std::string> entered) -> composition::Element
{
    std::vector<composition::Element> rows;
    for (int row = 0; row < 2; ++row)
    {
        std::vector<composition::Element> keysInRow;
        for (int column = 0; column < 5; ++column)
        {
            const int value = (row * 5 + column + 1) % 10;
            const char digit = static_cast<char>('0' + value);
            keysInRow.push_back(components::Button(std::string(1, digit),
                [entered, digit] {
                    if (entered.get().size() < length)
                        entered.update([digit](std::string& text) { text.push_back(digit); });
                })
                                    .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                                    .frame({ .width = 56.0F, .height = 44.0F })
                                    .id(composition::ElementKey{ std::string("pin.key.") + digit }));
        }
        rows.push_back(components::HStack(components::HStackOptions{ .spacing = theme::space::sm }, std::move(keysInRow)));
    }
    rows.back() = components::HStack(components::HStackOptions{ .spacing = theme::space::sm }, std::move(rows.back()),
        components::Button(
            [entered] {
                if (!entered.get().empty())
                    entered.update([](std::string& text) { text.pop_back(); });
            },
            components::Image(components::Image::SystemName{ "backspace" }))
            .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
            .frame({ .width = 56.0F, .height = 44.0F })
            .id(composition::ElementKey{ "pin.backspace" }));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::space::sm },
        std::move(rows))
        .focus_section();
}

}
