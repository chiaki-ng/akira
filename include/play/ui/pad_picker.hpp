#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/passed_input.hpp>

#include "app/strings.hpp"
#include "play/couch_table.hpp"
#include "play/pad_choice.hpp"
#include "play/ui/couch.hpp"

namespace akira::play::ui {

class PadPicker final : public ::arisa::composition::Component {
public:
    static constexpr float panelWidth = 1120.0F;
    static constexpr float maxCard = 300.0F;
    static constexpr float minCard = 148.0F;
    static constexpr float cardHeight = 252.0F;
    static constexpr float cardSpacing = 20.0F;
    static constexpr float artSide = 112.0F;
    static constexpr float scrim = 0.45F;
    static constexpr ::arisa::provider::Buttons passedButtons{ ::arisa::provider::Button::a, ::arisa::provider::Button::b,
        ::arisa::provider::Button::dpad_up, ::arisa::provider::Button::dpad_down, ::arisa::provider::Button::dpad_left,
        ::arisa::provider::Button::dpad_right };
    static constexpr ::arisa::runtime::PassedInput passedInput{ .touch = true };

    PadPicker(Couch& couch, const app::Strings& strings);

    bool operator==(const PadPicker& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto cardWidth(std::size_t pads) -> float;

private:
    auto card(const CouchTable::Pad& pad, bool selected, float width) const -> ::arisa::composition::Element;
    auto slotLine(const CouchTable::Pad& pad) const -> std::string;
    auto capability(PadChoice::Capability shown) const -> std::string;
    static auto chip(std::string text) -> ::arisa::composition::Element;
    static auto battery(int level) -> ::arisa::composition::Element;

    Couch* couch;
    const app::Strings* strings;
};

}
