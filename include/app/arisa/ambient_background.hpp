#pragma once

#include <array>
#include <chrono>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/size.hpp>

namespace akira::app::arisa {

class AmbientBackground final : public ::arisa::composition::Component {
public:
    enum class Presence { Full, Quiet };

    struct Sprite {
        const char* image;
        float aspect;
        bool accent;
    };

    static constexpr int motifCount = 28;
    static constexpr double cycleSeconds = 28.0;
    static constexpr std::chrono::milliseconds driftInterval{ AKIRA_PLATFORM_OUNCE ? 0 : 66 };
    static constexpr std::array<Sprite, 4> sprites{
        Sprite{ "img/brand/motif_circle.png", 1.0F, true },
        Sprite{ "img/brand/motif_square.png", 1.0F, false },
        Sprite{ "img/brand/motif_triangle.png", 473.0F / 420.0F, false },
        Sprite{ "img/brand/motif_cross.png", 472.0F / 470.0F, false },
    };

    explicit AmbientBackground(Presence presence);

    bool operator==(const AmbientBackground& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    struct Motif {
        int slot;
        float depth;
        double phase;
        int kind;
    };

    auto field(double seconds, ::arisa::composition::Size size, bool dark) const -> ::arisa::composition::Element;
    auto motif(const Motif& shown, double seconds, ::arisa::composition::Size size, bool dark) const
        -> ::arisa::composition::Element;
    static auto motifs() -> std::array<Motif, motifCount>;
    static auto edgePoint(double distance, ::arisa::composition::Size size) -> ::arisa::composition::Point;

    Presence presence;
};

}
