#pragma once

#include <cstdint>

namespace akira::input {

enum class Ds5EffectIntensity : std::uint8_t {
    Off    = 0,
    Strong = 1,
    Medium = 2,
    Weak   = 3,
};

inline float Ds5IntensityScale(Ds5EffectIntensity intensity)
{
    switch (intensity) {
        case Ds5EffectIntensity::Off:    return 0.0f;
        case Ds5EffectIntensity::Weak:   return 0.33f;
        case Ds5EffectIntensity::Medium: return 0.5f;
        case Ds5EffectIntensity::Strong: return 1.0f;
    }
    return 1.0f;
}

inline Ds5EffectIntensity Ds5IntensityFromWire(std::uint8_t value)
{
    switch (value) {
        case 0: return Ds5EffectIntensity::Off;
        case 1: return Ds5EffectIntensity::Strong;
        case 2: return Ds5EffectIntensity::Medium;
        case 3: return Ds5EffectIntensity::Weak;
        default: return Ds5EffectIntensity::Strong;
    }
}

} // namespace akira::input
