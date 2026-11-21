#include "app/arisa/ambient_background.hpp"

#include <arisa/components/environment.hpp>
#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/timeline_view.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour_value.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace motion = ::arisa::motion;
namespace theme = ::arisa::theme;

AmbientBackground::AmbientBackground(Presence presence)
    : presence(presence)
{
}

auto AmbientBackground::motifs() -> std::array<Motif, motifCount>
{
    std::array<Motif, motifCount> made{};
    for (int index = 0; index < motifCount; ++index)
        made[static_cast<std::size_t>(index)] = Motif{ .slot = index,
            .depth = 0.25F + (static_cast<float>((index * 5) % 7) / 12.0F),
            .phase = static_cast<double>((index * 11) % motifCount) / motifCount,
            .kind = index % static_cast<int>(sprites.size()) };
    std::ranges::sort(made, {}, &Motif::depth);
    return made;
}

auto AmbientBackground::body(composition::Context& context) const -> composition::Element
{
    const std::optional<theme::ColorScheme> scheme = context.environment(components::Environment::color_scheme);
    const bool dark = scheme.value_or(theme::ColorScheme::dark) == theme::ColorScheme::dark;
    const bool still = context.environment(components::Environment::reduce_motion);
    AmbientBackground held = *this;
    return composition::Element(components::GeometryReader([held, dark, still](const composition::GeometryProxy& proxy) {
        const composition::Size size = proxy.size();
        if (still)
            return held.field(0.0, size, dark);
        return composition::Element(components::TimelineView(components::TimelineSchedule::animation(),
            [held, size, dark](const components::TimelineView::Context& at) {
                const auto since = std::chrono::duration<double>(at.date.time_since_epoch()).count();
                return held.field(std::fmod(since, 3600.0), size, dark);
            }));
    }))
        .allows_hit_testing(false)
        .accessibility_hidden(true)
        .id(composition::ElementKey{ "ambient" });
}

auto AmbientBackground::field(double seconds, composition::Size size, bool dark) const -> composition::Element
{
    std::vector<composition::Element> shown;
    for (const Motif& each : motifs())
        shown.push_back(motif(each, seconds, size, dark));
    return components::ZStack(components::ZStackOptions{ .alignment = composition::Alignment::top_leading }, std::move(shown))
        .frame({ .width = size.width, .height = size.height, .alignment = composition::Alignment::top_leading });
}

auto AmbientBackground::edgePoint(double distance, composition::Size size) -> composition::Point
{
    const double width = size.width;
    const double height = size.height;
    if (distance < width)
        return composition::Point{ .x = static_cast<float>(distance), .y = 0.0F };
    if (distance < width + height)
        return composition::Point{ .x = size.width, .y = static_cast<float>(distance - width) };
    if (distance < (2.0 * width) + height)
        return composition::Point{ .x = static_cast<float>((2.0 * width) + height - distance), .y = size.height };
    return composition::Point{ .x = 0.0F, .y = static_cast<float>((2.0 * (width + height)) - distance) };
}

auto AmbientBackground::motif(const Motif& shown, double seconds, composition::Size size, bool dark) const
    -> composition::Element
{
    const double progress = std::fmod((seconds / cycleSeconds) + shown.phase, 1.0);
    const double turn = shown.phase * 2.0 * std::numbers::pi;
    const double rotation = (std::sin(turn) * 0.12) + (std::sin((seconds * 0.08) + turn) * 0.06);
    const double radial = 1.08 - (progress * 0.66);
    const double unit = std::min(size.width, size.height);
    const float side = static_cast<float>(unit * (0.034 + (shown.depth * 0.09)) * (0.72 + (progress * 0.60)));
    const composition::Point edge = edgePoint((shown.slot + 0.5) / motifCount * 2.0 * (size.width + size.height), size);
    const double x = (size.width / 2.0) + ((edge.x - (size.width / 2.0)) * radial);
    const double y = (size.height / 2.0) + ((edge.y - (size.height / 2.0)) * radial);
    const double reach = std::max(std::abs((x / size.width) - 0.5) * 2.0, std::abs((y / size.height) - 0.5) * 2.0);
    const double visible = presence == Presence::Quiet ? std::clamp((reach - 0.70) / 0.22, 0.0, 1.0) * 0.35 : 1.0;
    const double fade = std::min(1.0, progress / 0.12) * std::min(1.0, (1.0 - progress) / 0.18);
    const double alpha = ((dark ? 0.075 : 0.065) + (shown.depth * (dark ? 0.13 : 0.10))) * fade * visible;
    const Sprite& sprite = sprites[static_cast<std::size_t>(shown.kind)];
    const float height = side / sprite.aspect;
    return components::Image(sprite.image)
        .resizable()
        .rendering_mode(components::Image::TemplateRenderingMode::template_)
        .foreground_style(sprite.accent ? theme::ColourRole::accent : theme::ColourRole::label)
        .frame({ .width = side, .height = height })
        .rotation_effect(motion::Angle::radians(rotation))
        .opacity(static_cast<float>(std::max(alpha, 0.0)))
        .offset(static_cast<float>(x) - (side / 2.0F), static_cast<float>(y) - (height / 2.0F));
}

}
