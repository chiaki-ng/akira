#pragma once

#include <array>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/theme/colour.hpp>

namespace akira::app::arisa {

class BrandLockup final : public ::arisa::composition::Component {
public:
    enum class Motion { Idle, Connecting };

    static constexpr float logoAspect = 511.0F / 303.0F;
    static constexpr float mascotRadius = 0.22F;
    static constexpr float gap = 22.0F;
    static constexpr float crossX = 269.0F / 511.0F;
    static constexpr float crossY = 43.0F / 303.0F;
    static constexpr float crossWidth = 122.0F / 511.0F;
    static constexpr float crossHeight = 120.0F / 303.0F;
    static constexpr float crossPivotX = 0.6485F;
    static constexpr float crossPivotY = 0.3395F;
    static constexpr std::array<float, 2> eyeX{ 659.0F / 2044.0F, 816.0F / 2044.0F };
    static constexpr float eyeY = 307.0F / 1215.0F;
    static constexpr float eyeWidth = 68.0F / 2044.0F;
    static constexpr float eyeHeight = 177.0F / 1215.0F;
    static constexpr float fillX = 1691.0F / 2044.0F;
    static constexpr float fillBottom = 556.0F / 1215.0F;
    static constexpr float fillWidth = 290.0F / 2044.0F;
    static constexpr float fillHeight = 290.0F / 1215.0F;
    static constexpr float waveCentre = 0.13F;
    static constexpr float padding = 0.22F;
    static constexpr ::arisa::theme::Colour navy = ::arisa::theme::Colour::hex(0x182F4F);
    static constexpr ::arisa::theme::Colour lime = ::arisa::theme::Colour::hex(0xCEDD2A);
    static constexpr const char* baseImage = "img/brand/logo_base.png";
    static constexpr const char* crossImage = "img/brand/logo_cross.png";
    static constexpr const char* mascotImage = "img/icon.jpg";

    BrandLockup(float logoHeight, float mascotSide, Motion motion);

    bool operator==(const BrandLockup& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto frame(double seconds) const -> ::arisa::composition::Element;
    auto logo(double seconds) const -> ::arisa::composition::Element;
    auto waves(double seconds) const -> ::arisa::composition::Element;
    auto eyes(double seconds) const -> ::arisa::composition::Element;
    auto squareFill(double seconds) const -> ::arisa::composition::Element;
    auto cross(double seconds) const -> ::arisa::composition::Element;
    auto mascot(double seconds) const -> ::arisa::composition::Element;
    auto box(float x, float y, float width, float height, ::arisa::theme::Colour colour) const
        -> ::arisa::composition::Element;
    auto logoWidth() const -> float { return logoHeight * logoAspect; }
    auto pad() const -> float { return logoHeight * padding; }
    static auto smooth(double value) -> double;
    static auto blinkAt(double phase) -> double;

    float logoHeight;
    float mascotSide;
    Motion motion;
};

}
