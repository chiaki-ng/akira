#include "consoles/ui/console_editor.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/provider/text_input/text_input_traits.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/form_style.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
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

const composition::StateKey<ConsoleShelf::Edit> ConsoleEditor::draft{ "console-editor-draft", ConsoleShelf::Edit{} };

ConsoleEditor::ConsoleEditor(ConsoleShelf::Editing editing, ConsoleShelf& shelf, ConsoleCommands& commands,
    const app::Strings& strings)
    : editing(std::move(editing))
    , shelf(&shelf)
    , commands(&commands)
    , strings(&strings)
{
}

auto ConsoleEditor::capability(const char* key, const char* symbol, bool available) const -> composition::Element
{
    return components::HStack(components::HStackOptions{ .spacing = theme::Space{ 6.0F } },
        components::Image(components::Image::SystemName{ symbol }).font(theme::Font::system(13.0F)),
        components::Text(strings->text(key)).font(theme::Font::footnote))
        .foreground_style(available ? theme::ColourRole::label : theme::ColourRole::tertiary_label)
        .padding(theme::Insets::symmetric(theme::Space{ 10.0F }, theme::Space{ 5.0F }))
        .background(theme::ColourValue(theme::ColourRole::label).opacity(available ? 0.12F : 0.04F), theme::Shape::capsule());
}

auto ConsoleEditor::body(composition::Context& context) const -> composition::Element
{
    const auto values = context.state(draft, editing.values);
    ConsoleShelf* held = shelf;
    ConsoleCommands* sink = commands;
    const std::string name = editing.catalogName;
    auto cancel = [held] { held->edit(std::nullopt); };
    std::vector<composition::Element> sections;
    std::vector<composition::Element> connections;
    if (editing.address)
        connections.push_back(controls::TextField(strings->text("akira/hosts/edit_local_address"),
            composition::Binding<std::string>([values] { return values.get().address; },
                [values](std::string text) {
                    ConsoleShelf::Edit next = values.get();
                    next.address = std::move(text);
                    values.set(next);
                }),
            controls::TextFieldOptions{ .prompt = strings->text("akira/add_host/ip_placeholder") })
                                  .keyboard_type(provider::KeyboardType::numbers_and_punctuation)
                                  .id(composition::ElementKey{ "console-editor.address" }));
    connections.push_back(components::LabeledContent(strings->text("akira/hosts/edit_play_over"),
        components::HStack(components::HStackOptions{ .spacing = theme::Space{ 8.0F } },
            capability("akira/hosts/route_local", "wifi", !editing.remote),
            capability("akira/hosts/route_remote", "public", editing.remote))));
    sections.push_back(structure::Section(
        structure::Section::Labels{ .header = components::Text(strings->text("akira/hosts/edit_connections")) },
        std::move(connections)));
    std::vector<composition::Element> launch;
    if (editing.pin)
        launch.push_back(controls::TextField(strings->text("akira/host_settings/console_pin"),
            composition::Binding<std::string>([values] { return values.get().pin; },
                [values](std::string text) {
                    ConsoleShelf::Edit next = values.get();
                    next.pin = std::move(text);
                    values.set(next);
                }),
            controls::TextFieldOptions{ .prompt = strings->text("akira/host_settings/console_pin_placeholder") })
                             .keyboard_type(provider::KeyboardType::number_pad)
                             .id(composition::ElementKey{ "console-editor.pin" }));
    if (!editing.takion.empty())
    {
        const std::vector<std::string> labels = editing.takion;
        launch.push_back(controls::Picker(strings->text("akira/hosts/takion_version"),
            composition::Binding<int>([values] { return values.get().takion; },
                [values](int chosen) {
                    ConsoleShelf::Edit next = values.get();
                    next.takion = chosen;
                    values.set(next);
                }),
            [labels] {
                std::vector<composition::Element> items;
                for (std::size_t index = 0; index < labels.size(); ++index)
                    items.push_back(components::Text(labels[index]).tag(static_cast<int>(index)));
                return composition::Element(components::Group(std::move(items)));
            })
                             .id(composition::ElementKey{ "console-editor.takion" }));
    }
    if (!editing.codec.empty())
    {
        const std::vector<std::string> labels = editing.codec;
        launch.push_back(controls::Picker(strings->text("akira/hosts/video_codec"),
            composition::Binding<int>([values] { return values.get().codec; },
                [values](int chosen) {
                    ConsoleShelf::Edit next = values.get();
                    next.codec = chosen;
                    values.set(next);
                }),
            [labels] {
                std::vector<composition::Element> items;
                for (std::size_t index = 0; index < labels.size(); ++index)
                    items.push_back(components::Text(labels[index]).tag(static_cast<int>(index)));
                return composition::Element(components::Group(std::move(items)));
            })
                             .id(composition::ElementKey{ "console-editor.codec" }));
    }
    if (!launch.empty())
        sections.push_back(structure::Section(
            structure::Section::Labels{ .header = components::Text(strings->text("akira/hosts/edit_launch")) }, std::move(launch)));
    std::vector<composition::Element> buttons;
    if (editing.deletable)
        buttons.push_back(components::Button(strings->text("akira/hosts/edit_delete"), components::Image::SystemName{ "delete" },
            components::ButtonRole::destructive, [sink, name] { sink->askDelete(name); })
                              .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                              .id(composition::ElementKey{ "console-editor.delete" }));
    buttons.push_back(components::Spacer());
    buttons.push_back(components::Button(strings->text("akira/common/cancel"), components::ButtonRole::cancel, cancel)
                          .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                          .id(composition::ElementKey{ "console-editor.cancel" }));
    buttons.push_back(components::Button(strings->text("akira/hosts/edit_save"), [sink, name, values] { sink->saveEdit(name, values.get()); })
                          .button_style(components::ButtonStyle::glass_prominent())
                          .id(composition::ElementKey{ "console-editor.save" }));
    return components::VStack(
        components::VStackOptions{ .alignment = components::HorizontalAlignment::leading, .spacing = theme::space::lg },
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                               .spacing = theme::Space{ 2.0F } },
            components::Text(strings->text("akira/hosts/edit_title"))
                .font(theme::Font::system(titleSize, theme::Font::Weight::semibold)),
            components::Text(editing.name).font(theme::Font::callout).foreground_style(theme::ColourRole::secondary_label)),
        structure::Form(std::move(sections))
            .form_style(structure::FormStyle::cards())
            .scroll_content_background(composition::Visibility::hidden),
        components::HStack(components::HStackOptions{ .spacing = theme::space::md }, std::move(buttons)))
        .padding(theme::Space{ 24.0F })
        .frame({ .width = width })
        .on_exit_command(strings->text("akira/common/cancel"), cancel)
        .id(composition::ElementKey{ "console-editor" });
}

}
