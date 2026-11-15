#ifndef AKIRA_INPUT_RUMBLE_PROFILE_HPP
#define AKIRA_INPUT_RUMBLE_PROFILE_HPP

#include <cstdint>

#if AKIRA_PLATFORM_NX
#include <cstddef>
#include <string>

#include "input/ps_output.hpp"
#endif

namespace akira::input {


inline constexpr const char* kRumbleKeyDefault = "default";

inline constexpr const char* kRumbleKeySwitch = "switch";

inline constexpr const char* kRumbleKeyDualSense = "dualsense";
inline constexpr const char* kRumbleKeyJoyCon    = "joycon";
inline constexpr const char* kRumbleKeySwitchPro = "switchpro";
inline constexpr const char* kRumbleKeyGeneric   = "generic";

enum class PadOutputMode {
    Native = 0,
    Basic,
};

#if AKIRA_PLATFORM_NX
enum class PadCategory {
    DualSense = 0,
    JoyCon,
    SwitchPro,
    Generic,
};

inline const char* RumbleKeyForCategory(PadCategory category)
{
    switch (category) {
        case PadCategory::DualSense: return kRumbleKeyDualSense;
        case PadCategory::JoyCon:    return kRumbleKeyJoyCon;
        case PadCategory::SwitchPro: return kRumbleKeySwitchPro;
        case PadCategory::Generic:   return kRumbleKeyGeneric;
    }
    return kRumbleKeyGeneric;
}

inline PadCategory PadCategoryFor(std::uint16_t vendor_id, std::uint16_t product_id,
                                  bool switch_native, bool joycon)
{
    if (switch_native)
        return joycon ? PadCategory::JoyCon : PadCategory::SwitchPro;

    if (PadTakesDirectOutput(vendor_id, product_id))
        return PadCategory::DualSense;

    return PadCategory::Generic;
}

inline const char* PadCategoryName(PadCategory category)
{
    switch (category) {
        case PadCategory::DualSense: return "DualSense";
        case PadCategory::JoyCon:    return "Joy-Con";
        case PadCategory::SwitchPro: return "Switch Pro";
        case PadCategory::Generic:   return "Other controllers";
    }
    return "Other controllers";
}
#endif

enum class RumbleSource {
    Off = 0,
    Derived,
    Game,
};

enum class HapticIntensity {
    Off = 0,
    VeryWeak,
    Weak,
    Normal,
    Strong,
    VeryStrong,
};

struct RumbleProfile {
    float strength  = 1.0f;
    float ceiling   = 1.0f;

    bool  per_motor = true;

    float freq_low  = 160.0f;
    float freq_high = 320.0f;

    float envelope_attack = 0.60f;
    float envelope_decay  = 0.85f;

    HapticIntensity haptic_intensity = HapticIntensity::Normal;

    PadOutputMode output_mode = PadOutputMode::Native;

    RumbleSource rumble_source = RumbleSource::Derived;

    bool          lightbar_enabled = false;
    std::uint8_t  lightbar_r = 0x00;
    std::uint8_t  lightbar_g = 0x80;
    std::uint8_t  lightbar_b = 0xff;
};

inline RumbleProfile DefaultRumbleProfile()
{
    RumbleProfile p;
    p.strength  = 1.0f;
    p.ceiling   = 1.0f;
    p.per_motor = true;
    return p;
}

inline RumbleProfile SwitchRumbleProfile()
{
    RumbleProfile p;
    p.strength  = 1.0f;
    p.ceiling   = 0.63f;
    p.per_motor = false;
    p.freq_low  = 160.0f;
    p.freq_high = 320.0f;
    return p;
}

#if AKIRA_PLATFORM_NX
std::string RumbleKeyForModel(std::uint16_t vendor_id, std::uint16_t product_id);

std::string RumbleKeyForUnit(const std::uint8_t* address);

bool RumbleKeyIsUnit(const std::string& key);
bool RumbleKeyIsModel(const std::string& key);
bool RumbleKeyIsCategory(const std::string& key);

struct HapticRumble {
    bool          emit  = false;
    std::uint16_t left  = 0;
    std::uint16_t right = 0;

    std::uint32_t raw_left  = 0;
    std::uint32_t raw_right = 0;
};

inline constexpr std::uint32_t kHapticNoiseFloor = 100;

inline constexpr std::uint16_t kHapticMotorFloor = 3u << 8;

inline constexpr int kHapticBlockFloor = 0;
#endif

inline float HapticSampleGain(HapticIntensity intensity)
{
    switch (intensity) {
        case HapticIntensity::Off:        return 0.0f;
        case HapticIntensity::VeryWeak:   return 0.4f;
        case HapticIntensity::Weak:       return 0.7f;
        case HapticIntensity::Normal:     return 1.0f;
        case HapticIntensity::Strong:     return 1.6f;
        case HapticIntensity::VeryStrong: return 2.4f;
    }
    return 1.0f;
}

inline int HapticBaseForIntensity(HapticIntensity intensity)
{
    const float gain = HapticSampleGain(intensity);
    if (gain <= 0.0f)
        return 400;
    return (int)(50.0f / gain);
}

#if AKIRA_PLATFORM_NX
HapticRumble HapticAudioToRumble(const std::int16_t* stereo, std::size_t frames,
                                 HapticIntensity intensity, bool motor_stalls = true);
#endif

} // namespace akira::input

#endif // AKIRA_INPUT_RUMBLE_PROFILE_HPP
