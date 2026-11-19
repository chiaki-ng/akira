#include "app/views/borealis_overlay_palette.hpp"

#include "app/views/theme.hpp"

namespace akira::app::views {

auto BorealisOverlayPalette::colours() const -> Colours
{
    const Palette& palette = active();
    return Colours{
        .accent = colour(palette.accent),
        .warning = colour(palette.warning),
        .danger = colour(palette.danger),
        .success = colour(palette.success),
        .text = colour(palette.text),
        .backgroundDeep = colour(palette.backgroundDeep),
        .media = colour(palette.media),
    };
}

auto BorealisOverlayPalette::colour(NVGcolor color) -> Colour
{
    return Colour{ .r = color.r, .g = color.g, .b = color.b, .a = color.a };
}

}
