#pragma once

#include <cstddef>
#include <functional>
#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/theme/colour_value.hpp>

#include "app/strings.hpp"
#include "play/seat_board.hpp"
#include "play/ui/couch.hpp"

namespace akira::play::ui {

class PlayersPanel final : public ::arisa::composition::Component {
public:
    static constexpr float ledSize = 30.0F;
    static constexpr float rowSpacing = 10.0F;
    static constexpr float tileWidth = 140.0F;
    static constexpr float tileHeight = 128.0F;
    static constexpr float avatarSize = 56.0F;
    static constexpr std::size_t tilesPerRow = 4;

    PlayersPanel(Couch& couch, const app::Strings& strings);

    bool operator==(const PlayersPanel& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto playerColour(int player) -> ::arisa::theme::ColourValue;

private:
    auto heading(const Couch::View& view) const -> ::arisa::composition::Element;
    auto roster(const Couch::View& view) const -> ::arisa::composition::Element;
    auto row(const SeatBoard::Row& shown) const -> ::arisa::composition::Element;
    auto claiming(const Couch::View& view) const -> ::arisa::composition::Element;
    auto profiles(const Couch::View& view) const -> ::arisa::composition::Element;
    auto tile(const Couch::Profile& profile) const -> ::arisa::composition::Element;
    static auto led(int player, bool open) -> ::arisa::composition::Element;
    static auto tone(SeatBoard::Tone shown) -> ::arisa::theme::ColourValue;
    static auto action(std::string label, std::string key, std::function<void()> perform) -> ::arisa::composition::Element;

    Couch* couch;
    const app::Strings* strings;
};

}
