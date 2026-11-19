#pragma once

namespace akira::platform::video {

class OverlayPalette {
public:
    struct Colour {
        float r;
        float g;
        float b;
        float a;
    };

    struct Colours {
        Colour accent;
        Colour warning;
        Colour danger;
        Colour success;
        Colour text;
        Colour backgroundDeep;
        Colour media;
    };

    virtual ~OverlayPalette() = default;

    virtual auto colours() const -> Colours = 0;
};

}
