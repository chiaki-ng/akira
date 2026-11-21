#pragma once

#include <functional>
#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include "app/strings.hpp"
#include "consoles/ui/console_commands.hpp"
#include "consoles/ui/console_shelf.hpp"

namespace akira::consoles::ui {

class AddConsoleSheet final : public ::arisa::composition::Component {
public:
    static const ::arisa::composition::StateKey<std::string> address;
    static constexpr ::arisa::composition::StateKey<bool> ps5{ "add-console-ps5", true };
    static constexpr float width = 480.0F;
    static constexpr float titleSize = 28.0F;
    static constexpr float spacing = 18.0F;
    static constexpr float panelPadding = 18.0F;
    static constexpr float rowPadding = 16.0F;
    static constexpr float rowRadius = 20.0F;
    static constexpr float artHeight = 64.0F;
    static constexpr float artWidth = 44.0F;
    static constexpr float listHeight = 330.0F;

    AddConsoleSheet(ConsoleShelf& shelf, ConsoleCommands& commands, const app::Strings& strings);

    bool operator==(const AddConsoleSheet& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto modeKey(ConsoleShelf::AddMode mode) -> const char*;

private:
    auto modes(const ConsoleShelf::Adding& adding) const -> ::arisa::composition::Element;
    auto pane(const ConsoleShelf::View& view, const ConsoleShelf::Adding& adding,
        ::arisa::composition::StateHandle<std::string> typed, ::arisa::composition::StateHandle<bool> generation) const
        -> ::arisa::composition::Element;
    auto scanPanel(const ConsoleShelf::Scan& scan, bool found) const -> ::arisa::composition::Element;
    auto nearbyList(const ConsoleShelf::View& view) const -> ::arisa::composition::Element;
    auto manualEntry(::arisa::composition::StateHandle<std::string> typed,
        ::arisa::composition::StateHandle<bool> generation) const -> ::arisa::composition::Element;
    auto accountPicker(const ConsoleShelf::View& view, const ConsoleShelf::Adding& adding) const
        -> ::arisa::composition::Element;
    auto generations(::arisa::composition::Binding<bool> chosen, const char* key) const -> ::arisa::composition::Element;
    auto row(std::string name, bool ps5, ::arisa::composition::Element detail, std::string key, bool enabled,
        std::function<void()> chosen) const -> ::arisa::composition::Element;
    auto scanStatus(const ConsoleShelf::Scan& scan, bool found) const -> std::string;
    static auto ranges(std::string title, std::string values) -> ::arisa::composition::Element;
    static auto note(std::string text) -> ::arisa::composition::Element;

    ConsoleShelf* shelf;
    ConsoleCommands* commands;
    const app::Strings* strings;
};

}
