#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <arisa/async/task.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "play/button_board.hpp"
#include "play/ui/mapping.hpp"
#include "settings/ui/control_options.hpp"

namespace akira::play::ui {

class ButtonBoardView final : public ::arisa::composition::Component {
public:
    static constexpr float chipWidth = 224.0F;
    static constexpr float compactChipWidth = 320.0F;
    static constexpr float artWidth = 280.0F;
    static constexpr float glyphSize = 22.0F;
    static constexpr float targetSize = 26.0F;

    ButtonBoardView(Mapping& mapping, const app::Strings& strings, bool art);

    bool operator==(const ButtonBoardView& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto column(ButtonBoard::Side side) const -> ::arisa::composition::Element;
    auto chip(const ButtonBoard::Target& target) const -> ::arisa::composition::Element;
    static auto glyphs(const app::Strings& strings, const std::vector<std::uint64_t>& combo, float size)
        -> ::arisa::composition::Element;
    auto touchpad(const settings::ui::ControlOptions::Values& values) const -> ::arisa::composition::Element;
    auto enabled(std::uint32_t key, const settings::ui::ControlOptions::Values& values) const -> bool;
    auto toggle(std::uint32_t key, const std::string& label, bool on) const -> ::arisa::composition::Element;
    static auto captureSheet(Mapping* mapping, const app::Strings* strings, const Mapping::Status& status)
        -> ::arisa::composition::Element;
    static auto poll(Mapping* mapping) -> ::arisa::async::Task<void>;

    Mapping* mapping;
    const app::Strings* strings;
    bool art;
};

}
