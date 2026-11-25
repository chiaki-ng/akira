#pragma once

#include <functional>
#include <optional>
#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/theme/colour_value.hpp>

#include "app/strings.hpp"
#include "consoles/ui/console_commands.hpp"
#include "consoles/ui/console_shelf.hpp"
#include "consoles/ui/recent_shelf.hpp"

namespace akira::consoles::ui {

class ConsoleCard final : public ::arisa::composition::Component {
public:
    static constexpr float artWidth = 84.0F;
    static constexpr float artHeight = 112.0F;
    static constexpr float artShadowOpacity = 0.35F;
    static constexpr float artShadowRadius = 7.0F;
    static constexpr float artShadowDrop = 10.0F;
    static constexpr float spacing = 24.0F;
    static constexpr float horizontalPadding = 28.0F;
    static constexpr float verticalPadding = 18.0F;
    static constexpr float compactVerticalPadding = 20.0F;
    static constexpr float nameSize = 26.0F;
    static constexpr float compactNameSize = 24.0F;
    static constexpr float eyebrowSize = 13.0F;
    static constexpr float eyebrowTracking = 2.2F;
    static constexpr float tagRadius = 9.0F;
    static constexpr float iconSide = 44.0F;
    static constexpr float cardRadius = 26.0F;
    static constexpr float bottomShade = 0.72F;
    static constexpr float leadingShade = 0.5F;
    static constexpr float leadingMidShade = 0.32F;
    static constexpr float heroCentre = 0.64F;
    static constexpr float heroFigureHeight = 0.9F;
    static constexpr float heroFigureMiddle = 0.54F;
    static constexpr float heroRoomyWidth = 900.0F;
    static constexpr float heroSpareShift = 0.2F;
    static constexpr float logoWidth = 230.0F;
    static constexpr float logoHeight = 76.0F;
    static constexpr float compactLogoWidth = 170.0F;
    static constexpr float compactLogoHeight = 60.0F;
    static constexpr float titleInset = 28.0F;
    static constexpr float pillShadowOpacity = 0.55F;
    static constexpr float continueCover = 92.0F;
    static constexpr float continueCoverRadius = 12.0F;
    static constexpr float continueRadius = 18.0F;
    static constexpr float continueTextWidth = 220.0F;

    ConsoleCard(ConsoleShelf::Card card, bool compact, ConsoleCommands& commands, const app::Strings& strings,
        std::optional<RecentPlay::Title> lastPlayed = std::nullopt, std::optional<RecentShelf::Banner> banner = std::nullopt);

    bool operator==(const ConsoleCard& other) const;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto layered(::arisa::composition::Element content, const RecentShelf::Banner& shown) const
        -> ::arisa::composition::Element;
    auto continuePill(const RecentShelf::Banner& shown) const -> ::arisa::composition::Element;
    auto continueButton(const RecentPlay::Title& title) const -> ::arisa::composition::Element;
    auto playedWhen(const RecentPlay::Title& title) const -> std::optional<std::string>;
    auto header() const -> ::arisa::composition::Element;
    auto badge() const -> ::arisa::composition::Element;
    auto tag() const -> ::arisa::composition::Element;
    auto actions() const -> ::arisa::composition::Element;
    auto iconButton(const char* symbol, const char* label, std::string key, bool destructive,
        std::function<void()> pressed) const -> ::arisa::composition::Element;
    auto statusText() const -> std::string;
    auto statusColour() const -> ::arisa::theme::ColourValue;
    auto actionText() const -> std::string;
    auto missingLocally() const -> bool;

    ConsoleShelf::Card card;
    bool compact;
    ConsoleCommands* commands;
    const app::Strings* strings;
    std::optional<RecentPlay::Title> lastPlayed;
    std::optional<RecentShelf::Banner> banner;
};

}
