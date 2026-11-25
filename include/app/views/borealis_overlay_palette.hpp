#pragma once

#include <borealis/extern/nanovg/nanovg.h>

#include "platform/video/overlay_palette.hpp"

namespace akira::app::views {

class BorealisOverlayPalette : public platform::video::OverlayPalette {
public:
    auto colours() const -> Colours override;

private:
    static auto colour(NVGcolor color) -> Colour;
};

}
