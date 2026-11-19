#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "play/rumble_profile.hpp"
#include "platform/pads/extended/ps_output.hpp"

namespace akira::platform::extended {

enum class PadCategory {
    DualSense = 0,
    JoyCon,
    SwitchPro,
    Generic,
};

inline const char* RumbleKeyForCategory(PadCategory category)
{
    switch (category) {
        case PadCategory::DualSense: return input::kRumbleKeyDualSense;
        case PadCategory::JoyCon:    return input::kRumbleKeyJoyCon;
        case PadCategory::SwitchPro: return input::kRumbleKeySwitchPro;
        case PadCategory::Generic:   return input::kRumbleKeyGeneric;
    }
    return input::kRumbleKeyGeneric;
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

HapticRumble HapticAudioToRumble(const std::int16_t* stereo, std::size_t frames,
                                 input::HapticIntensity intensity, bool motor_stalls = true);

}
