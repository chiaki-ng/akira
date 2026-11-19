#include "app/arisa/overlay_palette.hpp"

namespace akira::app::arisa {

auto OverlayPalette::colours() const -> Colours
{
    return Colours{
        .accent = { .r = 0.04f, .g = 0.52f, .b = 1.0f, .a = 1.0f },
        .warning = { .r = 1.0f, .g = 0.62f, .b = 0.04f, .a = 1.0f },
        .danger = { .r = 1.0f, .g = 0.27f, .b = 0.23f, .a = 1.0f },
        .success = { .r = 0.19f, .g = 0.82f, .b = 0.35f, .a = 1.0f },
        .text = { .r = 1.0f, .g = 1.0f, .b = 1.0f, .a = 1.0f },
        .backgroundDeep = { .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f },
        .media = { .r = 0.37f, .g = 0.36f, .b = 0.9f, .a = 1.0f },
    };
}

}
