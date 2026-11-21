#include "consoles/ui/add_console_sheet.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/button_border_shape.hpp>
#include <arisa/components/button_sizing.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/control_size.hpp>
#include <arisa/components/glass_panel.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/text_attributes.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/provider/text_input/text_input_traits.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace akira::consoles::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace provider = ::arisa::provider;
namespace theme = ::arisa::theme;

const composition::StateKey<std::string> AddConsoleSheet::address{ "add-console-address", std::string() };

AddConsoleSheet::AddConsoleSheet(ConsoleShelf& shelf, ConsoleCommands& commands, const app::Strings& strings)
    : shelf(&shelf)
    , commands(&commands)
    , strings(&strings)
{
}

auto AddConsoleSheet::modeKey(ConsoleShelf::AddMode mode) -> const char*
{
    switch (mode)
    {
        case ConsoleShelf::AddMode::Automatic: return "akira/add_host/mode_automatic";
        case ConsoleShelf::AddMode::Manual: return "akira/add_host/mode_manual";
        case ConsoleShelf::AddMode::PlayStation: return "akira/add_host/mode_playstation";
    }
    return "akira/add_host/mode_automatic";
}

auto AddConsoleSheet::body(composition::Context& context) const -> composition::Element
{
    const ConsoleShelf::View& view = context.read(shelf->view());
    const ConsoleShelf::Adding adding = view.adding.value_or(ConsoleShelf::Adding{});
    const auto typed = context.state(address);
    const auto generation = context.state(ps5);
    ConsoleShelf* held = shelf;
    auto back = [held] { held->openAdd(false); };
    std::vector<composition::Element> rows;
    rows.push_back(components::Text(strings->text("akira/add_host/find_title"))
                       .font(theme::Font::system(titleSize, theme::Font::Weight::semibold))
                       .padding(theme::Edge::bottom, theme::Space{ 6.0F })
                       .id(composition::ElementKey{ "add-console.title" }));
    rows.push_back(modes(adding));
    if (adding.failure)
        rows.push_back(components::Text(*adding.failure)
                           .font(theme::Font::caption)
                           .foreground_style(theme::ColourRole::red)
                           .multiline_text_alignment(composition::TextAlignment::center)
                           .id(composition::ElementKey{ "add-console.failure" }));
    rows.push_back(pane(view, adding, typed, generation));
    rows.push_back(components::Button(strings->text("akira/common/back"), back)
                       .button_style(components::ButtonStyle::plain())
                       .foreground_style(theme::ColourRole::secondary_label)
                       .id(composition::ElementKey{ "add-console.back" }));
    return components::VStack(components::VStackOptions{ .spacing = theme::Space{ spacing } }, std::move(rows))
        .padding(theme::Space{ 28.0F })
        .frame({ .width = width + 56.0F })
        .on_exit_command(strings->text("akira/common/back"), back)
        .id(composition::ElementKey{ "add-console" });
}

auto AddConsoleSheet::modes(const ConsoleShelf::Adding& adding) const -> composition::Element
{
    ConsoleCommands* held = commands;
    std::vector<std::pair<ConsoleShelf::AddMode, std::string>> labels;
    for (const ConsoleShelf::AddMode mode : shelf->modes())
        labels.emplace_back(mode, strings->text(modeKey(mode)));
    composition::Binding<ConsoleShelf::AddMode> chosen([mode = adding.mode] { return mode; },
        [held](ConsoleShelf::AddMode mode) { held->chooseAddMode(mode); });
    return controls::Picker(strings->text("akira/add_host/find_title"), std::move(chosen),
        [labels] {
            std::vector<composition::Element> items;
            for (const auto& [mode, label] : labels)
                items.push_back(components::Text(label).tag(mode));
            return composition::Element(components::Group(std::move(items)));
        })
        .picker_style(controls::PickerStyle::segmented())
        .labels_hidden()
        .id(composition::ElementKey{ "add-console.mode" });
}

auto AddConsoleSheet::pane(const ConsoleShelf::View& view, const ConsoleShelf::Adding& adding,
    composition::StateHandle<std::string> typed, composition::StateHandle<bool> generation) const -> composition::Element
{
    switch (adding.mode)
    {
        case ConsoleShelf::AddMode::Manual: return manualEntry(typed, generation);
        case ConsoleShelf::AddMode::PlayStation: return accountPicker(view, adding);
        case ConsoleShelf::AddMode::Automatic: break;
    }
    const bool paired = std::ranges::any_of(view.cards, [](const ConsoleShelf::Card& card) {
        const ConsoleShelf::Presence presence = ConsoleShelf::presenceOf(card);
        return card.badge != ConsoleEntry::Badge::Remote
            && (presence == ConsoleShelf::Presence::Ready || presence == ConsoleShelf::Presence::RestMode);
    });
    std::vector<composition::Element> rows;
    rows.push_back(scanPanel(view.scan, !view.nearby.empty() || paired));
    if (!view.nearby.empty())
        rows.push_back(nearbyList(view));
    else if (paired)
        rows.push_back(note(strings->text("akira/add_host/all_paired")));
    return components::VStack(components::VStackOptions{ .spacing = theme::Space{ spacing } }, std::move(rows));
}

auto AddConsoleSheet::scanStatus(const ConsoleShelf::Scan& scan, bool found) const -> std::string
{
    if (!scan.running)
        return strings->text("akira/add_host/scan_paused");
    if (scan.broadcast.empty())
        return strings->text("akira/add_host/scan_no_network");
    if (found)
        return strings->text("akira/add_host/scan_scanning");
    if (scan.searched)
        return strings->text("akira/add_host/scan_still_looking");
    return strings->text("akira/add_host/scan_looking");
}

auto AddConsoleSheet::ranges(std::string title, std::string values) -> composition::Element
{
    return components::VStack(
        components::VStackOptions{ .alignment = components::HorizontalAlignment::leading, .spacing = theme::Space{ 4.0F } },
        components::Text(std::move(title))
            .font(theme::Font::caption)
            .font_weight(theme::Font::Weight::semibold)
            .foreground_style(theme::ColourRole::secondary_label),
        components::Text(std::move(values)).font(theme::Font::caption.monospaced()));
}

auto AddConsoleSheet::scanPanel(const ConsoleShelf::Scan& scan, bool found) const -> composition::Element
{
    std::vector<composition::Element> status;
    if (scan.running)
        status.push_back(controls::ProgressView().control_size(components::ControlSize::small));
    status.push_back(components::Text(scanStatus(scan, found)).font(theme::Font::callout).font_weight(theme::Font::Weight::medium));
    status.push_back(components::Spacer());
    std::vector<composition::Element> rows;
    rows.push_back(components::HStack(components::HStackOptions{ .spacing = theme::Space{ 10.0F } }, std::move(status)));
    if (!scan.broadcast.empty())
        rows.push_back(ranges(strings->text("akira/add_host/broadcast"), scan.broadcast));
    if (!scan.unicast.empty())
    {
        std::string joined;
        for (const std::string& range : scan.unicast)
            joined += (joined.empty() ? "" : "\n") + range;
        rows.push_back(ranges(strings->text("akira/add_host/unicast"), std::move(joined)));
    }
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 12.0F } },
        std::move(rows))
        .padding(theme::Space{ panelPadding })
        .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading })
        .glass_panel(components::PanelRadius::medium)
        .id(composition::ElementKey{ "add-console.scan" });
}

auto AddConsoleSheet::row(std::string name, bool consolePs5, composition::Element detail, std::string key, bool enabled,
    std::function<void()> chosen) const -> composition::Element
{
    return components::Button(std::move(chosen),
        components::HStack(components::HStackOptions{ .spacing = theme::Space{ 16.0F } },
            components::Image(consolePs5 ? "img/console/ps5.png" : "img/console/ps4.png")
                .resizable()
                .aspect_ratio(std::nullopt, composition::ContentMode::fit)
                .frame({ .width = artWidth, .height = artHeight }),
            components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                   .spacing = theme::Space{ 5.0F } },
                components::Text(std::move(name)).font(theme::Font::headline).line_limit(1), std::move(detail)),
            components::Spacer(),
            components::Image(components::Image::SystemName{ "chevron_right" })
                .font(theme::Font::caption)
                .foreground_style(theme::ColourRole::secondary_label))
            .padding(theme::Space{ rowPadding })
            .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading }))
        .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
        .button_sizing(components::ButtonSizing::flexible)
        .button_border_shape(components::ButtonBorderShape::rounded_rectangle(rowRadius))
        .disabled(!enabled)
        .id(composition::ElementKey{ std::move(key) });
}

auto AddConsoleSheet::nearbyList(const ConsoleShelf::View& view) const -> composition::Element
{
    ConsoleShelf* held = shelf;
    ConsoleCommands* sink = commands;
    std::vector<composition::Element> rows;
    for (const ConsoleShelf::Nearby& found : view.nearby)
    {
        composition::Element detail = components::VStack(
            components::VStackOptions{ .alignment = components::HorizontalAlignment::leading, .spacing = theme::Space{ 3.0F } },
            components::Text(strings->text(found.standby ? "akira/hosts/rest_mode" : "akira/hosts/ready"))
                .font(theme::Font::caption)
                .foreground_style(found.standby ? theme::ColourRole::orange : theme::ColourRole::green),
            components::Text(found.address).font(theme::Font::caption2).foreground_style(theme::ColourRole::secondary_label));
        rows.push_back(row(found.name, found.ps5, std::move(detail), "add-console.nearby." + found.catalogName, true,
            [held, sink, name = found.catalogName] {
                held->openAdd(false);
                sink->primary(name);
            }));
    }
    return components::ScrollView(
        components::VStack(components::VStackOptions{ .spacing = theme::Space{ 10.0F } }, std::move(rows)).padding(theme::Space{ 4.0F }))
        .frame({ .max_height = listHeight })
        .id(composition::ElementKey{ "add-console.nearby" });
}

auto AddConsoleSheet::generations(composition::Binding<bool> chosen, const char* key) const -> composition::Element
{
    const std::string ps5Label = strings->text("akira/common/ps5");
    const std::string ps4Label = strings->text("akira/common/ps4");
    return controls::Picker(strings->text("akira/add_host/console_type"), std::move(chosen),
        [ps5Label, ps4Label] {
            return composition::Element(
                components::Group(components::Text(ps5Label).tag(true), components::Text(ps4Label).tag(false)));
        })
        .picker_style(controls::PickerStyle::segmented())
        .labels_hidden()
        .id(composition::ElementKey{ key });
}

auto AddConsoleSheet::manualEntry(composition::StateHandle<std::string> typed, composition::StateHandle<bool> generation) const
    -> composition::Element
{
    ConsoleCommands* held = commands;
    auto submit = [held, typed, generation] {
        if (!typed.get().empty())
            held->addByAddress(typed.get(), generation.get());
    };
    return components::VStack(components::VStackOptions{ .spacing = theme::Space{ 14.0F } },
        generations(composition::Binding<bool>(generation), "add-console.kind"),
        components::HStack(components::HStackOptions{ .spacing = theme::Space{ 10.0F } },
            controls::TextField(strings->text("akira/add_host/address_prompt"), composition::Binding<std::string>(typed),
                controls::TextFieldOptions{ .prompt = strings->text("akira/add_host/address_prompt") })
                .keyboard_type(provider::KeyboardType::numbers_and_punctuation)
                .labels_hidden()
                .on_submit(submit)
                .id(composition::ElementKey{ "add-console.address" }),
            components::Button(strings->text("akira/add_host/continue"), submit)
                .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                .control_size(components::ControlSize::large)
                .disabled(typed.get().empty())
                .id(composition::ElementKey{ "add-console.continue" })))
        .padding(theme::Space{ panelPadding })
        .glass_panel(components::PanelRadius::medium)
        .id(composition::ElementKey{ "add-console.manual" });
}

auto AddConsoleSheet::accountPicker(const ConsoleShelf::View& view, const ConsoleShelf::Adding& adding) const
    -> composition::Element
{
    ConsoleShelf* held = shelf;
    ConsoleCommands* sink = commands;
    std::vector<composition::Element> rows;
    rows.push_back(generations(composition::Binding<bool>([chosen = adding.accountPs5] { return chosen; },
                                   [held](bool value) { held->chooseAccountGeneration(value); }),
        "add-console.account-kind"));
    switch (adding.lookup)
    {
        case ConsoleShelf::Lookup::Idle:
        case ConsoleShelf::Lookup::Loading:
            rows.push_back(controls::ProgressView(strings->text("akira/add_host/account_loading"))
                               .control_size(components::ControlSize::small)
                               .padding(theme::Space{ 16.0F })
                               .id(composition::ElementKey{ "add-console.account-loading" }));
            break;
        case ConsoleShelf::Lookup::Failed:
            rows.push_back(note(strings->text("akira/add_host/account_failed")));
            rows.push_back(components::Button(strings->text("akira/common/retry"), [sink] { sink->findRemote(); })
                               .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                               .id(composition::ElementKey{ "add-console.account-retry" }));
            break;
        case ConsoleShelf::Lookup::Ready:
        {
            std::vector<composition::Element> listed;
            for (const ConsoleShelf::AccountConsole& console : view.account)
            {
                if (console.ps5 != adding.accountPs5)
                    continue;
                composition::Element detail = components::Text(strings->text(
                    console.remotePlay ? "akira/add_host/account_register" : "akira/add_host/account_enable_rp"))
                                                  .font(theme::Font::caption)
                                                  .foreground_style(theme::ColourRole::secondary_label);
                listed.push_back(row(console.name, console.ps5, std::move(detail), "add-console.account." + console.catalogName,
                    console.remotePlay, [held, sink, name = console.catalogName] {
                        held->openAdd(false);
                        sink->registerOverInternet(name);
                    }));
            }
            if (listed.empty())
                rows.push_back(note(strings->text("akira/add_host/account_none")));
            else
                rows.push_back(components::ScrollView(components::VStack(
                                   components::VStackOptions{ .spacing = theme::Space{ 10.0F } }, std::move(listed))
                                                          .padding(theme::Space{ 4.0F }))
                                   .frame({ .max_height = listHeight }));
            break;
        }
    }
    return components::VStack(components::VStackOptions{ .spacing = theme::Space{ 14.0F } }, std::move(rows))
        .id(composition::ElementKey{ "add-console.account" });
}

auto AddConsoleSheet::note(std::string text) -> composition::Element
{
    return components::Text(std::move(text))
        .font(theme::Font::callout)
        .foreground_style(theme::ColourRole::secondary_label)
        .multiline_text_alignment(composition::TextAlignment::center);
}

}
