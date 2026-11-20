#pragma once

namespace akira::play {

class TouchClaim {
public:
    struct Fraction {
        float x = 0.0f;
        float y = 0.0f;

        friend bool operator==(const Fraction&, const Fraction&) = default;
    };

    virtual ~TouchClaim() = default;

    virtual auto grab(Fraction at) -> bool = 0;
    virtual void drag(Fraction at) = 0;
    virtual void release() = 0;
};

}
