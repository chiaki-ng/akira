#include "platform/pads/extended/rumble_keys.hpp"

#include <cstdio>
#include <cstdlib>

namespace akira::platform::extended {

namespace {

constexpr std::size_t kUnitKeyLength  = 17;
constexpr std::size_t kModelKeyLength = 9;

std::uint32_t ScaleForIntensity(std::uint32_t value, play::HapticIntensity intensity)
{
    switch (intensity) {
        case play::HapticIntensity::VeryWeak:   return value / 5;
        case play::HapticIntensity::Weak:       return value / 2;
        case play::HapticIntensity::Strong:     return value * 2;
        case play::HapticIntensity::VeryStrong: return value * 5;
        case play::HapticIntensity::Normal:
        default:                          return value;
    }
}

std::uint16_t Clamp16(std::uint32_t value)
{
    return value > 0xffffu ? (std::uint16_t)0xffffu : (std::uint16_t)value;
}

std::uint16_t ApplyMotorFloor(std::uint16_t value)
{
    if (value == 0)
        return 0;
    return value < kHapticMotorFloor ? kHapticMotorFloor : value;
}

} // namespace

std::string RumbleKeyForModel(std::uint16_t vendor_id, std::uint16_t product_id)
{
    char buf[kModelKeyLength + 1];
    std::snprintf(buf, sizeof(buf), "%04x:%04x", vendor_id, product_id);
    return std::string(buf);
}

std::string RumbleKeyForUnit(const std::uint8_t* address)
{
    if (address == nullptr)
        return std::string();

    char buf[kUnitKeyLength + 1];
    std::snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
                  address[0], address[1], address[2],
                  address[3], address[4], address[5]);
    return std::string(buf);
}

bool RumbleKeyIsUnit(const std::string& key)
{
    if (key.size() != kUnitKeyLength)
        return false;

    for (std::size_t i = 2; i < key.size(); i += 3) {
        if (key[i] != ':')
            return false;
    }
    return true;
}

bool RumbleKeyIsModel(const std::string& key)
{
    return key.size() == kModelKeyLength && key[4] == ':';
}

bool RumbleKeyIsCategory(const std::string& key)
{
    return key == play::kRumbleKeyDualSense
        || key == play::kRumbleKeyJoyCon
        || key == play::kRumbleKeySwitchPro
        || key == play::kRumbleKeyGeneric;
}

HapticRumble HapticAudioToRumble(const std::int16_t* stereo, std::size_t frames,
                                 play::HapticIntensity intensity, bool motor_stalls)
{
    HapticRumble out;

    if (stereo == nullptr || frames == 0 || intensity == play::HapticIntensity::Off)
        return out;

    std::uint32_t peak_left  = 0;
    std::uint32_t peak_right = 0;

    for (std::size_t i = 0; i < frames; i++) {
        const std::int32_t l = stereo[i * 2];
        const std::int32_t r = stereo[i * 2 + 1];

        const std::uint32_t al = (std::uint32_t)(l < 0 ? -l : l);
        const std::uint32_t ar = (std::uint32_t)(r < 0 ? -r : r);

        if (al > peak_left)  peak_left  = al;
        if (ar > peak_right) peak_right = ar;
    }

    std::uint32_t left  = peak_left  * 2;
    std::uint32_t right = peak_right * 2;

    if (left  <= kHapticNoiseFloor) left  = 0;
    if (right <= kHapticNoiseFloor) right = 0;

    if (left == 0 && right == 0)
        return out;

    out.raw_left  = left;
    out.raw_right = right;
    out.left  = Clamp16(ScaleForIntensity(left,  intensity));
    out.right = Clamp16(ScaleForIntensity(right, intensity));

    if (motor_stalls) {
        out.left  = ApplyMotorFloor(out.left);
        out.right = ApplyMotorFloor(out.right);
    }
    out.emit  = true;

    return out;
}

} // namespace akira::platform::extended
