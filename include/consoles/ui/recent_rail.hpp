#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "consoles/recent_play.hpp"
#include "consoles/ui/console_commands.hpp"
#include "consoles/ui/recent_shelf.hpp"

namespace akira::consoles::ui {

class RecentRail final : public ::arisa::composition::Component {
public:
    static constexpr float artSize = 148.0F;
    static constexpr float artRadius = 12.0F;
    static constexpr float tileSpacing = 16.0F;
    static constexpr float playSize = 34.0F;
    static constexpr float playInset = 8.0F;

    RecentRail(RecentShelf& shelf, ConsoleCommands& commands, const app::Strings& strings);

    bool operator==(const RecentRail& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto tile(const std::string& catalogName, const RecentPlay::Title& title, RecentPlay::Clock::time_point now) const
        -> ::arisa::composition::Element;
    auto when(const RecentPlay::Title& title, RecentPlay::Clock::time_point now) const -> std::string;

    RecentShelf* shelf;
    ConsoleCommands* commands;
    const app::Strings* strings;
};

}
