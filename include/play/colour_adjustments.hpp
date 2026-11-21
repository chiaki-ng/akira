#pragma once

#include <array>
#include <cstddef>

namespace akira::play {

class ColourAdjustments {
public:
    enum class Channel { Brightness, Contrast, Saturation, Gamma };

    struct Range {
        double lower;
        double upper;
    };

    static constexpr double step = 0.05;
    static constexpr std::array<Channel, 4> channels{ Channel::Brightness, Channel::Contrast, Channel::Saturation,
        Channel::Gamma };

    ColourAdjustments() = default;
    ColourAdjustments(double brightness, double contrast, double saturation, double gamma);

    static auto range(Channel channel) -> Range;
    static auto neutral(Channel channel) -> double;

    auto value(Channel channel) const -> double;
    auto with(Channel channel, double chosen) const -> ColourAdjustments;
    auto isNeutral() const -> bool;
    auto shaderConstants() const -> std::array<float, 4>;

    friend bool operator==(const ColourAdjustments&, const ColourAdjustments&) = default;

private:
    static auto index(Channel channel) -> std::size_t;
    static auto clamped(Channel channel, double chosen) -> double;

    std::array<double, 4> values{ 0.0, 1.0, 1.0, 1.0 };
};

}
