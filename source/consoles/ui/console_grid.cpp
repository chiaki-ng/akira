#include "consoles/ui/console_grid.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/control_size.hpp>
#include <arisa/components/content_unavailable_view.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/composition/presentation_sizing.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/presentation/alert.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>

#include <algorithm>
#include <type_traits>
#include <utility>

#include "consoles/ui/add_console_sheet.hpp"
#include "consoles/ui/console_card.hpp"
#include "consoles/ui/console_field_sheet.hpp"
#include "consoles/ui/pin_pad.hpp"
#include "consoles/ui/registration_progress.hpp"

namespace akira::consoles::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace provider = ::arisa::provider;
namespace theme = ::arisa::theme;

ConsoleGrid::ConsoleGrid(ConsoleShelf& shelf, ConsoleCommands& commands, const app::Strings& strings)
    : shelf(&shelf)
    , commands(&commands)
    , strings(&strings)
{
}

bool ConsoleGrid::operator==(const ConsoleGrid& other) const
{
    return shelf == other.shelf && commands == other.commands && strings == other.strings;
}

auto ConsoleGrid::columnsFor(float width, std::size_t cards) -> int
{
    return width >= twoColumnWidth && cards > 1 ? 2 : 1;
}

auto ConsoleGrid::body(composition::Context& context) const -> composition::Element
{
    const ConsoleShelf::View& view = context.read(shelf->view());
    const auto width = context.state(available);
    composition::Element screen =
        components::ScrollView(content(view, width.get())
                                   .padding(theme::Insets{ .top = theme::Space{ 4.0F }, .bottom = theme::Space{ 30.0F } })
                                   .frame({ .max_width = composition::infinity,
                                       .alignment = composition::Alignment::top_leading }))
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
            .on_geometry_change(
                std::type_identity<float>{}, [](const composition::GeometryProxy& proxy) { return proxy.size().width; },
                [width](const float& measured) { width.set(measured); });
    if (view.hasProfile)
        screen = std::move(screen).on_command(provider::Button::plus, strings->text("akira/hosts/add_console"),
            [shelf = shelf] { shelf->openAdd(true); });
    return prompted(std::move(screen), view);
}

auto ConsoleGrid::content(const ConsoleShelf::View& view, float width) const -> composition::Element
{
    if (!view.hasProfile)
        return components::VStack(components::VStackOptions{ .spacing = theme::space::lg },
            components::ContentUnavailableView(strings->text("akira/hosts/setup_account"), "person",
                components::Text(strings->text("akira/hosts/profile_required"))),
            components::Button(strings->text("akira/hosts/setup_account"), components::Image::SystemName{ "person_add" },
                [commands = commands] { commands->setUpAccount(); })
                .button_style(components::ButtonStyle::glass_prominent())
                .control_size(components::ControlSize::large)
                .id(composition::ElementKey{ "consoles.set-up" }))
            .frame({ .max_width = composition::infinity, .min_height = emptyHeight })
            .id(composition::ElementKey{ "consoles.no-profile" });
    if (view.cards.empty())
        return components::VStack(components::VStackOptions{ .spacing = theme::space::lg },
            components::ContentUnavailableView(strings->text("akira/hosts/no_consoles"), "tv",
                components::Text(strings->text("akira/hosts/no_consoles_hint"))),
            addButton())
            .frame({ .max_width = composition::infinity, .min_height = emptyHeight })
            .id(composition::ElementKey{ "consoles.empty" });
    return grid(view.cards, width);
}

auto ConsoleGrid::grid(const std::vector<ConsoleShelf::Card>& cards, float width) const -> composition::Element
{
    const std::size_t perRow = static_cast<std::size_t>(columnsFor(width, cards.size()));
    const bool compact = perRow > 1;
    const float cardWidth = (width - spacing) / 2.0F;
    std::vector<composition::Element> rows;
    for (std::size_t start = 0; start < cards.size(); start += perRow)
    {
        std::vector<composition::Element> row;
        for (std::size_t index = start; index < std::min(start + perRow, cards.size()); ++index)
        {
            composition::Element card = ConsoleCard(cards[index], compact, *commands, *strings);
            row.push_back(compact ? std::move(card).frame({ .width = cardWidth })
                                  : std::move(card).frame({ .max_width = composition::infinity }));
        }
        rows.push_back(components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::top,
                                              .spacing = theme::Space{ spacing } },
            std::move(row)));
    }
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ spacing } },
        std::move(rows))
        .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading })
        .focus_section()
        .id(composition::ElementKey{ "consoles.grid" });
}

auto ConsoleGrid::addButton() const -> composition::Element
{
    return components::Button(strings->text("akira/hosts/add_console"), components::Image::SystemName{ "add" },
        [shelf = shelf] { shelf->openAdd(true); })
        .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
        .control_size(components::ControlSize::large)
        .id(composition::ElementKey{ "consoles.add" });
}

auto ConsoleGrid::asked(ConsoleShelf::Ask kind) const -> composition::Binding<bool>
{
    return composition::Binding<bool>(
        [shelf = shelf, kind] {
            const auto& prompt = shelf->view().get().prompt;
            return prompt.has_value() && prompt->ask == kind;
        },
        [shelf = shelf, commands = commands, kind](bool shown) {
            const auto& prompt = shelf->view().get().prompt;
            if (shown || !prompt.has_value() || prompt->ask != kind)
                return;
            if (kind == ConsoleShelf::Ask::Pin)
                commands->cancelRegistration(prompt->catalogName);
            else
                shelf->dismiss();
        });
}

auto ConsoleGrid::adding() const -> composition::Binding<bool>
{
    return composition::Binding<bool>([shelf = shelf] { return shelf->view().get().adding; },
        [shelf = shelf](bool shown) { shelf->openAdd(shown); });
}

auto ConsoleGrid::prompted(composition::Element screen, const ConsoleShelf::View& view) const -> composition::Element
{
    composition::Element shown = sheets(dialogs(std::move(screen), view), view);
    const bool registering = view.registering.has_value();
    composition::Element progress = components::Group();
    if (registering)
        progress = RegistrationProgress(*view.registering, *commands, *strings);
    return std::move(shown).overlay(registering, composition::OverlayLayer::sheet, std::move(progress));
}

auto ConsoleGrid::choices(const std::optional<ConsoleShelf::Prompt>& prompt, ConsoleShelf* shelf,
    ConsoleCommands* commands, const app::Strings* strings) -> composition::Element
{
    std::vector<composition::Element> buttons;
    if (prompt)
    {
        for (std::size_t index = 0; index < prompt->choices.size(); ++index)
        {
            const std::string label = prompt->choices[index];
            const int choice = static_cast<int>(index);
            if (prompt->ask == ConsoleShelf::Ask::Link)
                buttons.push_back(components::Button(label,
                    [commands, name = prompt->catalogName, label] { commands->link(name, label); }));
            else
                buttons.push_back(components::Button(choice == prompt->selected ? label + "  \xe2\x9c\x93" : label,
                    [commands, name = prompt->catalogName, choice] { commands->pinTakion(name, choice); }));
        }
    }
    buttons.push_back(components::Button(strings->text("akira/common/cancel"), components::ButtonRole::cancel,
        [shelf] { shelf->dismiss(); }));
    return components::Group(std::move(buttons));
}

auto ConsoleGrid::dialogs(composition::Element screen, const ConsoleShelf::View& view) const -> composition::Element
{
    const std::string name = view.prompt ? view.prompt->catalogName : std::string();
    const std::string text = view.prompt ? view.prompt->text : std::string();
    const std::size_t split = text.find("\n\n");
    const std::string title = split == std::string::npos ? text : text.substr(0, split);
    const std::string message = split == std::string::npos ? std::string() : text.substr(split + 2);
    return std::move(screen)
        .alert(title, asked(ConsoleShelf::Ask::Notice),
            [shelf = shelf, ok = strings->text("akira/common/ok")] {
                return components::Button(ok, components::ButtonRole::cancel, [shelf] { shelf->dismiss(); });
            },
            [message] { return components::Text(message); })
        .confirmation_dialog(strings->text("akira/hosts/register_how"), asked(ConsoleShelf::Ask::RegisterHow),
            composition::Visibility::visible,
            [commands = commands, name, strings = strings] {
                return components::Group(
                    components::Button(strings->text("akira/hosts/register_auto"),
                        [commands, name] { commands->registerOverInternet(name); }),
                    components::Button(strings->text("akira/hosts/register_pin"),
                        [commands, name] { commands->askPin(name); }),
                    components::Button(strings->text("akira/common/cancel"), components::ButtonRole::cancel,
                        [commands, name] { commands->cancelRegistration(name); }));
            })
        .confirmation_dialog(title, asked(ConsoleShelf::Ask::Delete), composition::Visibility::visible,
            [commands = commands, shelf = shelf, name, strings = strings] {
                return components::Group(
                    components::Button(strings->text("akira/common/delete"), components::ButtonRole::destructive,
                        [commands, name] { commands->remove(name); }),
                    components::Button(strings->text("akira/common/cancel"), components::ButtonRole::cancel,
                        [shelf] { shelf->dismiss(); }));
            },
            [message] { return components::Text(message); })
        .confirmation_dialog(strings->text("akira/hosts/takion_version"), asked(ConsoleShelf::Ask::TakionVersion),
            composition::Visibility::visible,
            [prompt = view.prompt, shelf = shelf, commands = commands, strings = strings] {
                return choices(prompt, shelf, commands, strings);
            })
        .confirmation_dialog(strings->text("akira/hosts/select_host_to_link"), asked(ConsoleShelf::Ask::Link),
            composition::Visibility::visible,
            [prompt = view.prompt, shelf = shelf, commands = commands, strings = strings] {
                return choices(prompt, shelf, commands, strings);
            });
}

auto ConsoleGrid::sheets(composition::Element screen, const ConsoleShelf::View& view) const -> composition::Element
{
    const std::optional<ConsoleShelf::Prompt> prompt = view.prompt;
    const std::optional<ConsoleShelf::Card> target =
        prompt ? shelf->card(prompt->catalogName) : std::optional<ConsoleShelf::Card>();
    return std::move(screen)
        .sheet(asked(ConsoleShelf::Ask::Pin),
            [commands = commands, strings = strings, prompt, target] {
                return composition::Element(PinPad(prompt ? prompt->catalogName : std::string(),
                    target ? target->name : std::string(), target && target->ps5, *commands, *strings))
                    .presentation_sizing(composition::PresentationSizing::fitted);
            })
        .sheet(asked(ConsoleShelf::Ask::ConsolePin),
            [shelf = shelf, commands = commands, strings = strings, prompt] {
                return composition::Element(ConsoleFieldSheet(ConsoleFieldSheet::Field::ConsolePin,
                    prompt ? prompt->catalogName : std::string(), prompt ? prompt->text : std::string(), *shelf,
                    *commands, *strings))
                    .presentation_sizing(composition::PresentationSizing::fitted);
            })
        .sheet(asked(ConsoleShelf::Ask::Address),
            [shelf = shelf, commands = commands, strings = strings, prompt] {
                return composition::Element(ConsoleFieldSheet(ConsoleFieldSheet::Field::Address,
                    prompt ? prompt->catalogName : std::string(), prompt ? prompt->text : std::string(), *shelf,
                    *commands, *strings))
                    .presentation_sizing(composition::PresentationSizing::fitted);
            })
        .sheet(adding(), [shelf = shelf, commands = commands, strings = strings] {
            return composition::Element(AddConsoleSheet(*shelf, *commands, *strings))
                    .presentation_sizing(composition::PresentationSizing::fitted);
        });
}

}
