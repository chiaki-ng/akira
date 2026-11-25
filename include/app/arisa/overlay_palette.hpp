#pragma once

#include "platform/video/overlay_palette.hpp"

namespace akira::app::arisa {

class OverlayPalette final : public platform::video::OverlayPalette {
public:
    auto colours() const -> Colours override;
};

}
