#pragma once

#include <optional>
#include <string>
#include <vector>

#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>

#include "app/strings.hpp"
#include "consoles/ui/console_commands.hpp"
#include "consoles/ui/console_shelf.hpp"
#include "consoles/ui/recent_shelf.hpp"

namespace akira::consoles::ui {

class ConsoleGrid final : public ::arisa::composition::Component {
public:
    static constexpr float twoColumnWidth = 1100.0F;
    static constexpr float spacing = 18.0F;
    static constexpr float emptyHeight = 420.0F;
    static constexpr ::arisa::composition::StateKey<float> available{ "console-grid-width", 0.0F };

    static constexpr float recentGap = 26.0F;

    ConsoleGrid(ConsoleShelf& shelf, ConsoleCommands& commands, const app::Strings& strings);
    ConsoleGrid(ConsoleShelf& shelf, ConsoleCommands& commands, RecentShelf* recent, const app::Strings& strings);

    bool operator==(const ConsoleGrid& other) const;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto columnsFor(float width, std::size_t cards) -> int;

private:
    auto content(const ConsoleShelf::View& view, float width) const -> ::arisa::composition::Element;
    auto grid(const std::vector<ConsoleShelf::Card>& cards, float width) const -> ::arisa::composition::Element;
    auto addButton() const -> ::arisa::composition::Element;
    auto prompted(::arisa::composition::Element screen, const ConsoleShelf::View& view) const
        -> ::arisa::composition::Element;
    auto dialogs(::arisa::composition::Element screen, const ConsoleShelf::View& view) const
        -> ::arisa::composition::Element;
    auto sheets(::arisa::composition::Element screen, const ConsoleShelf::View& view) const
        -> ::arisa::composition::Element;
    auto asked(ConsoleShelf::Ask kind) const -> ::arisa::composition::Binding<bool>;
    auto adding() const -> ::arisa::composition::Binding<bool>;
    static auto choices(const std::optional<ConsoleShelf::Prompt>& prompt, ConsoleShelf* shelf,
        ConsoleCommands* commands, const app::Strings* strings) -> ::arisa::composition::Element;

    ConsoleShelf* shelf;
    ConsoleCommands* commands;
    RecentShelf* recent = nullptr;
    const app::Strings* strings;
};

}
