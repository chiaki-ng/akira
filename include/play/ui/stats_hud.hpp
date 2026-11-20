#pragma once

#include <string>
#include <vector>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>

#include "platform/video/overlay_palette.hpp"
#include "play/stats_hud_placement.hpp"
#include "play/stats_readout.hpp"
#include "play/ui/stats_board.hpp"

namespace akira::play::ui {

class StatsHud final : public ::arisa::composition::Component {
public:
    static constexpr float radius = 10.0F;
    static constexpr float compactHeight = 36.0F;
    static constexpr float compactUpscaleHeight = 22.0F;
    static constexpr float compactInset = 14.0F;
    static constexpr float fullWidth = 452.0F;
    static constexpr float fullInset = 16.0F;
    static constexpr float headerHeight = 34.0F;
    static constexpr float columnGap = 14.0F;
    static constexpr float rowHeight = 20.0F;
    static constexpr float footerHeight = 28.0F;
    static constexpr float barHeight = 7.0F;

    StatsHud(StatsBoard& board, StatsHudPlacement& placement, platform::video::OverlayPalette::Colours palette);

    bool operator==(const StatsHud& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto compact(const StatsReadout& readout) const -> ::arisa::composition::Element;
    auto full(const StatsReadout& readout) const -> ::arisa::composition::Element;
    auto compactCell(const StatsReadout::Cell& cell, bool first) const -> ::arisa::composition::Element;
    auto compactUpscale(const StatsReadout& readout) const -> ::arisa::composition::Element;
    auto header(const StatsReadout& readout) const -> ::arisa::composition::Element;
    auto column(std::string title, const std::vector<StatsReadout::Cell>& cells,
        const StatsReadout* upscaled) const -> ::arisa::composition::Element;
    auto row(const StatsReadout::Cell& cell) const -> ::arisa::composition::Element;
    auto upscaleRow(const StatsReadout& readout) const -> ::arisa::composition::Element;
    auto latency(const StatsReadout& readout) const -> ::arisa::composition::Element;
    auto footer(const StatsReadout& readout) const -> ::arisa::composition::Element;
    auto panel(::arisa::composition::Element content) const -> ::arisa::composition::Element;
    auto ink(float alpha) const -> ::arisa::theme::ColourValue;
    auto toned(StatsReadout::Tone tone) const -> ::arisa::theme::ColourValue;
    auto text(std::string shown, float size, ::arisa::theme::ColourValue shade) const -> ::arisa::composition::Element;
    static auto colour(platform::video::OverlayPalette::Colour source) -> ::arisa::theme::Colour;
    static auto dot(::arisa::theme::ColourValue shade, float size) -> ::arisa::composition::Element;
    static auto rule(::arisa::theme::ColourValue shade, float width, float height) -> ::arisa::composition::Element;

    StatsBoard* board;
    StatsHudPlacement* placement;
    platform::video::OverlayPalette::Colours palette;
};

}
