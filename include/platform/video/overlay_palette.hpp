#pragma once

namespace akira::platform::video {

class OverlayPalette {
public:
    struct Colour {
        float r;
        float g;
        float b;
        float a;

        friend bool operator==(const Colour&, const Colour&) = default;
    };

    struct Colours {
        Colour accent;
        Colour warning;
        Colour danger;
        Colour success;
        Colour text;
        Colour backgroundDeep;
        Colour media;

        friend bool operator==(const Colours&, const Colours&) = default;
    };

    virtual ~OverlayPalette() = default;

    virtual auto colours() const -> Colours = 0;
};

}
