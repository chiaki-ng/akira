#include "play/colour_adjustments.hpp"

#include <algorithm>
#include <cmath>

namespace akira::play {

ColourAdjustments::ColourAdjustments(double brightness, double contrast, double saturation, double gamma)
    : values{ clamped(Channel::Brightness, brightness), clamped(Channel::Contrast, contrast),
        clamped(Channel::Saturation, saturation), clamped(Channel::Gamma, gamma) }
{
}

auto ColourAdjustments::range(Channel channel) -> Range
{
    switch (channel)
    {
        case Channel::Brightness: return { .lower = -0.25, .upper = 0.25 };
        case Channel::Contrast: return { .lower = 0.5, .upper = 1.5 };
        case Channel::Saturation: return { .lower = 0.0, .upper = 2.0 };
        case Channel::Gamma: break;
    }
    return { .lower = 0.5, .upper = 2.0 };
}

auto ColourAdjustments::neutral(Channel channel) -> double
{
    return channel == Channel::Brightness ? 0.0 : 1.0;
}

auto ColourAdjustments::index(Channel channel) -> std::size_t
{
    return static_cast<std::size_t>(channel);
}

auto ColourAdjustments::clamped(Channel channel, double chosen) -> double
{
    const Range limits = range(channel);
    if (!std::isfinite(chosen))
        return neutral(channel);
    const double stepped = std::round(chosen / step) * step;
    return std::clamp(stepped, limits.lower, limits.upper);
}

auto ColourAdjustments::value(Channel channel) const -> double
{
    return values[index(channel)];
}

auto ColourAdjustments::with(Channel channel, double chosen) const -> ColourAdjustments
{
    ColourAdjustments changed = *this;
    changed.values[index(channel)] = clamped(channel, chosen);
    return changed;
}

auto ColourAdjustments::isNeutral() const -> bool
{
    return *this == ColourAdjustments();
}

auto ColourAdjustments::shaderConstants() const -> std::array<float, 4>
{
    return { static_cast<float>(value(Channel::Brightness)), static_cast<float>(value(Channel::Contrast)),
        static_cast<float>(value(Channel::Saturation)), static_cast<float>(value(Channel::Gamma)) };
}

}
