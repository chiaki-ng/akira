#include "app/arisa/brand_lockup.hpp"

#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/timeline_view.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace graphics = ::arisa::graphics;
namespace motion = ::arisa::motion;
namespace theme = ::arisa::theme;

BrandLockup::BrandLockup(float logoHeight, float mascotSide, Motion motion)
    : logoHeight(logoHeight)
    , mascotSide(mascotSide)
    , motion(motion)
{
}

auto BrandLockup::body(composition::Context& context) const -> composition::Element
{
    if (context.environment(components::Environment::reduce_motion))
        return frame(0.0);
    BrandLockup held = *this;
    return components::TimelineView(components::TimelineSchedule::animation(),
        [held](const components::TimelineView::Context& at) {
            const auto since = std::chrono::duration<double>(at.date.time_since_epoch()).count();
            return held.frame(std::fmod(since, 3600.0));
        });
}

auto BrandLockup::frame(double seconds) const -> composition::Element
{
    const float side = mascotSide;
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                                  .spacing = theme::Space{ gap } },
        logo(seconds),
        components::Text("\xc3\x97")
            .font(theme::Font::system(26.0F, theme::Font::Weight::light))
            .foreground_style(theme::ColourRole::secondary_label),
        mascot(seconds).frame({ .width = side, .height = side }).clip_shape(theme::Shape::rounded(side * mascotRadius)))
        .accessibility_hidden(true)
        .id(composition::ElementKey{ "brand.lockup" });
}

auto BrandLockup::logo(double seconds) const -> composition::Element
{
    const float width = logoWidth();
    const float bob = static_cast<float>(std::sin(seconds * std::numbers::pi / 3.2)) * logoHeight / 120.0F;
    composition::Element art = components::ZStack(components::ZStackOptions{ .alignment = composition::Alignment::top_leading },
        components::Image(baseImage)
            .resizable()
            .interpolation(components::Image::Interpolation::high)
            .frame({ .width = width, .height = logoHeight })
            .offset(pad(), pad()),
        eyes(seconds), squareFill(seconds), waves(seconds), cross(seconds))
                                   .frame({ .width = width + (pad() * 2.0F),
                                       .height = logoHeight + (pad() * 2.0F),
                                       .alignment = composition::Alignment::top_leading })
                                   .offset(-pad(), bob - pad());
    return std::move(art)
        .frame({ .width = width, .height = logoHeight, .alignment = composition::Alignment::top_leading })
        .id(composition::ElementKey{ "brand.logo" });
}

auto BrandLockup::waves(double seconds) const -> composition::Element
{
    const float width = logoWidth();
    const double period = motion == Motion::Connecting ? 2.3 : 3.6;
    const float centre = pad() + (width * waveCentre);
    std::vector<composition::Element> arcs;
    for (int index = 0; index < 3; ++index)
    {
        const double progress = std::fmod((seconds / period) + (static_cast<double>(index) / 3.0), 1.0);
        const float travel = static_cast<float>(progress);
        const float spread = width * (0.040F + (0.105F * travel));
        const float crest = pad() + (logoHeight * (0.16F - (0.24F * travel)));
        const float bend = logoHeight * (0.025F + (0.045F * travel));
        const float alpha =
            static_cast<float>(std::pow(std::max(0.0, std::sin(std::numbers::pi * progress)), 0.8) * 0.85);
        graphics::Path arc;
        arc.move(composition::Point{ .x = centre - spread, .y = crest + bend });
        arc.add_quad_curve(composition::Point{ .x = centre + spread, .y = crest + bend },
            composition::Point{ .x = centre, .y = crest - bend });
        const composition::FrameSpec canvas{ .width = width + (pad() * 2.0F),
            .height = logoHeight + (pad() * 2.0F),
            .alignment = composition::Alignment::top_leading };
        arcs.push_back(components::ZStack(components::ZStackOptions{ .alignment = composition::Alignment::top_leading },
            components::ShapeView(arc)
                .stroke(graphics::ShapeStyle(lime),
                    graphics::StrokeStyle{ .line_width = logoHeight * 0.075F, .line_cap = graphics::LineCap::round })
                .frame(canvas),
            components::ShapeView(arc)
                .stroke(graphics::ShapeStyle(navy),
                    graphics::StrokeStyle{ .line_width = logoHeight * 0.038F, .line_cap = graphics::LineCap::round })
                .frame(canvas))
                           .opacity(alpha));
    }
    return components::ZStack(components::ZStackOptions{ .alignment = composition::Alignment::top_leading }, std::move(arcs))
        .frame({ .width = width + (pad() * 2.0F),
            .height = logoHeight + (pad() * 2.0F),
            .alignment = composition::Alignment::top_leading });
}

auto BrandLockup::eyes(double seconds) const -> composition::Element
{
    const double phase = std::fmod(seconds, 6.24);
    const float closure = static_cast<float>(std::max(blinkAt(phase - 3.4), blinkAt(phase - 3.86)));
    const float width = logoWidth();
    const float lid = logoHeight * eyeHeight * closure / 2.0F;
    std::vector<composition::Element> lids;
    for (const float x : eyeX)
    {
        lids.push_back(box(width * x, logoHeight * eyeY, width * eyeWidth, lid, navy));
        lids.push_back(box(width * x, (logoHeight * (eyeY + eyeHeight)) - lid, width * eyeWidth, lid, navy));
    }
    return components::ZStack(components::ZStackOptions{ .alignment = composition::Alignment::top_leading }, std::move(lids));
}

auto BrandLockup::squareFill(double seconds) const -> composition::Element
{
    const float level = static_cast<float>((1.0 - std::cos(seconds * 2.0 * std::numbers::pi / 6.4)) / 2.0);
    const float width = logoWidth();
    const float height = logoHeight * fillHeight * level;
    return box(width * fillX, (logoHeight * fillBottom) - height, width * fillWidth, height, lime);
}

auto BrandLockup::cross(double seconds) const -> composition::Element
{
    const double phase = std::fmod(seconds, 5.4);
    const double angle = phase < 3.4 ? std::sin(phase * std::numbers::pi / 1.7) * 5.0 : smooth((phase - 3.4) / 1.2) * 360.0;
    const float hop = phase >= 3.4 && phase <= 4.6
        ? static_cast<float>(-std::sin((phase - 3.4) / 1.2 * std::numbers::pi)) * logoHeight * 7.0F / 120.0F
        : 0.0F;
    const float width = logoWidth();
    const float spriteWidth = width * crossWidth;
    const float spriteHeight = logoHeight * crossHeight;
    const motion::UnitPoint pivot{ ((crossPivotX - crossX) * width) / spriteWidth,
        ((crossPivotY - crossY) * logoHeight) / spriteHeight };
    return components::Image(crossImage)
        .resizable()
        .interpolation(components::Image::Interpolation::high)
        .frame({ .width = spriteWidth, .height = spriteHeight })
        .rotation_effect(motion::Angle::degrees(angle), pivot)
        .offset(pad() + (width * crossX), pad() + (logoHeight * crossY) + hop);
}

auto BrandLockup::mascot(double seconds) const -> composition::Element
{
    const float breath = 1.0F + (0.012F * static_cast<float>(std::sin(seconds * 2.4)));
    const double sway = 0.03 * std::sin(seconds * 1.6);
    return components::Image(mascotImage)
        .resizable()
        .aspect_ratio(std::nullopt, composition::ContentMode::fill)
        .scale_effect(1.06F, motion::UnitPoint::center)
        .scale_effect(1.0F, breath, motion::UnitPoint::bottom)
        .rotation_effect(motion::Angle::radians(sway), motion::UnitPoint::bottom);
}

auto BrandLockup::box(float x, float y, float width, float height, theme::Colour colour) const -> composition::Element
{
    return composition::Element(components::Group())
        .frame({ .width = width, .height = std::max(height, 0.0F) })
        .background(theme::ColourValue(colour))
        .offset(pad() + x, pad() + y);
}

auto BrandLockup::smooth(double value) -> double
{
    const double clamped = std::clamp(value, 0.0, 1.0);
    return clamped * clamped * (3.0 - (2.0 * clamped));
}

auto BrandLockup::blinkAt(double phase) -> double
{
    if (phase < 0.0 || phase >= 0.28)
        return 0.0;
    const double closing = phase < 0.10 ? smooth(phase / 0.10) : 1.0 - smooth((phase - 0.10) / 0.18);
    return closing * 0.94;
}

}
