#ifndef AKIRA_INPUT_PS_OUTPUT_HPP
#define AKIRA_INPUT_PS_OUTPUT_HPP

#include <cstdint>

#include "play/ds5_intensity.hpp"

#include <cstddef>
#include <cstring>

namespace akira::platform::extended {

inline constexpr std::size_t kDs5EffectsBytes = 47;

inline constexpr std::size_t kDs5TriggerParamBytes = 10;

inline constexpr std::size_t kDs5BluetoothFrameBytes   = 78;
inline constexpr std::size_t kDs5BluetoothPayloadStart = 3;
inline constexpr std::uint8_t kDs5OutputTag            = 0x10;

enum Ds5EffectsOffset : std::size_t {
    Ds5Effects_EnableBits1  = 0,
    Ds5Effects_EnableBits2  = 1,
    Ds5Effects_RumbleRight  = 2,
    Ds5Effects_RumbleLeft   = 3,
    Ds5Effects_HeadphoneVol = 4,
    Ds5Effects_SpeakerVol   = 5,
    Ds5Effects_AudioControl = 7,
    Ds5Effects_RightTrigger = 10,
    Ds5Effects_LeftTrigger  = 21,
    Ds5Effects_Intensity     = 36,
    Ds5Effects_AudioControl2 = 37,
    Ds5Effects_EnableBits3  = 38,
    Ds5Effects_LedRed       = 44,
    Ds5Effects_LedGreen     = 45,
    Ds5Effects_LedBlue      = 46,
};

enum Ds5Enable1 : std::uint8_t {
    Ds5Enable1_RumbleLegacy = 1u << 0,

    Ds5Enable1_HapticsSelect = 1u << 1,

    Ds5Enable1_RightTrigger = 1u << 2,
    Ds5Enable1_LeftTrigger  = 1u << 3,

    Ds5Enable1_HeadphoneVol  = 1u << 4,
    Ds5Enable1_SpeakerVol    = 1u << 5,
    Ds5Enable1_AudioControl  = 1u << 7,
};

enum Ds5Enable2 : std::uint8_t {
    Ds5Enable2_LedColor      = 1u << 2,

    Ds5Enable2_Intensity     = 1u << 6,
    Ds5Enable2_AudioControl2 = 1u << 7,
};

enum Ds5Enable3 : std::uint8_t {
    Ds5Enable3_Rumble          = 1u << 2,
    Ds5Enable3_RumbleNotHaptic = 1u << 3,
};


inline std::uint8_t Ds5IntensityByte(input::Ds5EffectIntensity vibration,
                                     input::Ds5EffectIntensity trigger)
{
    std::uint8_t low = 0x00;
    switch (vibration) {
        case input::Ds5EffectIntensity::Strong: low = 0x00; break;
        case input::Ds5EffectIntensity::Medium: low = 0x02; break;
        case input::Ds5EffectIntensity::Weak:   low = 0x03; break;
        case input::Ds5EffectIntensity::Off:    low = 0x0f; break;
    }

    std::uint8_t high = 0x00;
    switch (trigger) {
        case input::Ds5EffectIntensity::Strong: high = 0x00; break;
        case input::Ds5EffectIntensity::Medium: high = 0x60; break;
        case input::Ds5EffectIntensity::Weak:   high = 0x90; break;
        case input::Ds5EffectIntensity::Off:    high = 0xf0; break;
    }

    return (std::uint8_t)(high | low);
}

inline constexpr std::uint8_t kDs5IntensityFull = 0x00;


inline std::uint32_t Ds5Crc32(std::uint32_t crc, const std::uint8_t* data, std::size_t len)
{
    for (std::size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xedb88320u) : (crc >> 1);
    }
    return crc;
}

inline void StampDs5Crc(std::uint8_t* out, std::size_t len)
{
    const std::uint8_t header = 0xa2;
    std::uint32_t crc = 0xffffffffu;
    crc = Ds5Crc32(crc, &header, 1);
    crc = Ds5Crc32(crc, out, len - 4);
    crc ^= 0xffffffffu;

    out[len - 4] = (std::uint8_t)(crc & 0xff);
    out[len - 3] = (std::uint8_t)((crc >> 8) & 0xff);
    out[len - 2] = (std::uint8_t)((crc >> 16) & 0xff);
    out[len - 1] = (std::uint8_t)((crc >> 24) & 0xff);
}

enum class Ds5FrameLayout {
    Tagged,
    MissionCtl,
};

inline std::size_t BuildDs5BluetoothFrameAs(Ds5FrameLayout layout, std::uint8_t seq,
                                            const std::uint8_t* effects, std::size_t effects_len,
                                            std::uint8_t* out, std::size_t out_len)
{
    if (effects == nullptr || out == nullptr)
        return 0;
    if (effects_len > kDs5EffectsBytes || out_len < kDs5BluetoothFrameBytes)
        return 0;

    std::memset(out, 0, kDs5BluetoothFrameBytes);
    out[0] = 0x31;

    if (layout == Ds5FrameLayout::MissionCtl) {
        out[1] = 0x02;
        std::memcpy(out + 2, effects, effects_len);
    } else {
        out[1] = (std::uint8_t)((seq & 0x0f) << 4);
        out[2] = kDs5OutputTag;
        std::memcpy(out + kDs5BluetoothPayloadStart, effects, effects_len);
    }

    StampDs5Crc(out, kDs5BluetoothFrameBytes);
    return kDs5BluetoothFrameBytes;
}

inline std::size_t BuildDs5BluetoothFrame(std::uint8_t seq,
                                          const std::uint8_t* effects, std::size_t effects_len,
                                          std::uint8_t* out, std::size_t out_len)
{
    if (effects == nullptr || out == nullptr)
        return 0;
    if (effects_len > kDs5EffectsBytes || out_len < kDs5BluetoothFrameBytes)
        return 0;

    std::memset(out, 0, kDs5BluetoothFrameBytes);
    out[0] = 0x31;
    out[1] = (std::uint8_t)((seq & 0x0f) << 4);
    out[2] = kDs5OutputTag;
    std::memcpy(out + kDs5BluetoothPayloadStart, effects, effects_len);

    StampDs5Crc(out, kDs5BluetoothFrameBytes);
    return kDs5BluetoothFrameBytes;
}

inline void ApplyDs5VibrationMode(std::uint8_t* effects,
                                  std::uint8_t left, std::uint8_t right,
                                  bool legacy_vibration)
{
    effects[Ds5Effects_EnableBits1] &= (std::uint8_t)~(Ds5Enable1_RumbleLegacy |
                                                       Ds5Enable1_HapticsSelect);
    effects[Ds5Effects_EnableBits3] &= (std::uint8_t)~(Ds5Enable3_Rumble |
                                                       Ds5Enable3_RumbleNotHaptic);

    effects[Ds5Effects_RumbleLeft]  = left;
    effects[Ds5Effects_RumbleRight] = right;

    if ((left | right) == 0) {
        effects[Ds5Effects_EnableBits1] |= Ds5Enable1_RumbleLegacy;
        return;
    }

    effects[Ds5Effects_EnableBits1] |= Ds5Enable1_HapticsSelect;

    if (legacy_vibration)
        effects[Ds5Effects_EnableBits1] |= Ds5Enable1_RumbleLegacy;
    else
        effects[Ds5Effects_EnableBits3] |= Ds5Enable3_Rumble;
}

inline std::size_t BuildDs5RumbleFrame(std::uint8_t seq,
                                       std::uint8_t left, std::uint8_t right,
                                       std::uint8_t* out, std::size_t out_len,
                                       bool legacy_vibration = false,
                                       Ds5FrameLayout layout = Ds5FrameLayout::Tagged,
                                       std::uint8_t intensity = kDs5IntensityFull)
{
    std::uint8_t effects[kDs5EffectsBytes] = {};


    effects[Ds5Effects_EnableBits2] = Ds5Enable2_Intensity;
    effects[Ds5Effects_Intensity]   = intensity;

    ApplyDs5VibrationMode(effects, left, right, legacy_vibration);

    return BuildDs5BluetoothFrame(seq, effects, sizeof(effects), out, out_len);
}

inline std::size_t BuildDs5AudioEnableFrame(std::uint8_t seq,
                                            std::uint8_t* out, std::size_t out_len)
{
    std::uint8_t effects[kDs5EffectsBytes] = {};

    effects[Ds5Effects_EnableBits1] = Ds5Enable1_AudioControl
                                    | Ds5Enable1_SpeakerVol
                                    | Ds5Enable1_HeadphoneVol;
    effects[Ds5Effects_EnableBits2] = Ds5Enable2_AudioControl2;

    effects[Ds5Effects_HeadphoneVol] = 0x7f;
    effects[Ds5Effects_SpeakerVol]   = 0x64;

    effects[Ds5Effects_AudioControl]  = 0x30;
    effects[Ds5Effects_AudioControl2] = 0x04;

    return BuildDs5BluetoothFrame(seq, effects, sizeof(effects), out, out_len);
}

inline constexpr std::size_t kDs5Haptic32FrameBytes  = 142;
inline constexpr std::size_t kDs5Haptic32SampleBytes = 64;

inline std::size_t BuildDs5Haptic32Frame(std::uint8_t seq, std::uint8_t packet_counter,
                                         const std::int8_t* samples, std::size_t sample_bytes,
                                         std::uint8_t* out, std::size_t out_len)
{
    if (samples == nullptr || out == nullptr)
        return 0;
    if (sample_bytes != kDs5Haptic32SampleBytes || out_len < kDs5Haptic32FrameBytes)
        return 0;

    std::memset(out, 0, kDs5Haptic32FrameBytes);

    out[0] = 0x32;
    out[1] = (std::uint8_t)((seq & 0x0f) << 4);

    out[2] = 0x11 | 0x80;
    out[3] = 7;
    out[4] = 0xfe;
    out[5] = out[6] = out[7] = out[8] = 0;
    out[9] = 0xff;
    out[10] = packet_counter;

    out[11] = 0x12 | 0x80;
    out[12] = (std::uint8_t)kDs5Haptic32SampleBytes;
    std::memcpy(out + 13, samples, sample_bytes);

    StampDs5Crc(out, kDs5Haptic32FrameBytes);
    return kDs5Haptic32FrameBytes;
}

inline std::size_t BuildDs5LightbarFrame(std::uint8_t seq,
                                         std::uint8_t red, std::uint8_t green,
                                         std::uint8_t blue,
                                         std::uint8_t* out, std::size_t out_len,
                                         Ds5FrameLayout layout = Ds5FrameLayout::Tagged)
{
    std::uint8_t effects[kDs5EffectsBytes] = {};

    effects[Ds5Effects_EnableBits2] = Ds5Enable2_LedColor;
    effects[Ds5Effects_LedRed]      = red;
    effects[Ds5Effects_LedGreen]    = green;
    effects[Ds5Effects_LedBlue]     = blue;

    return BuildDs5BluetoothFrameAs(layout, seq, effects, sizeof(effects), out, out_len);
}

inline std::size_t BuildDs5StateFrame(std::uint8_t seq,
                                      std::uint8_t left, std::uint8_t right,
                                      std::uint8_t left_trigger_type,
                                      const std::uint8_t* left_params,
                                      std::uint8_t right_trigger_type,
                                      const std::uint8_t* right_params,
                                      std::uint8_t* out, std::size_t out_len,
                                      bool legacy_vibration = false,
                                      Ds5FrameLayout layout = Ds5FrameLayout::Tagged,
                                      std::uint8_t intensity = kDs5IntensityFull)
{
    std::uint8_t effects[kDs5EffectsBytes] = {};

    ApplyDs5VibrationMode(effects, left, right, legacy_vibration);

    effects[Ds5Effects_EnableBits2] = Ds5Enable2_Intensity;
    effects[Ds5Effects_Intensity]   = intensity;

    effects[Ds5Effects_EnableBits1] |= Ds5Enable1_LeftTrigger | Ds5Enable1_RightTrigger;

    effects[Ds5Effects_LeftTrigger] = left_trigger_type;
    if (left_params != nullptr)
        std::memcpy(effects + Ds5Effects_LeftTrigger + 1, left_params, kDs5TriggerParamBytes);

    effects[Ds5Effects_RightTrigger] = right_trigger_type;
    if (right_params != nullptr)
        std::memcpy(effects + Ds5Effects_RightTrigger + 1, right_params, kDs5TriggerParamBytes);

    return BuildDs5BluetoothFrameAs(layout, seq, effects, sizeof(effects), out, out_len);
}

inline std::size_t BuildDs5HapticsRestoreFrame(std::uint8_t seq,
                                               std::uint8_t* out, std::size_t out_len,
                                               Ds5FrameLayout layout = Ds5FrameLayout::Tagged)
{
    std::uint8_t effects[kDs5EffectsBytes] = {};

    ApplyDs5VibrationMode(effects, 0, 0, false);

    effects[Ds5Effects_EnableBits2] = Ds5Enable2_Intensity;
    effects[Ds5Effects_Intensity]   = kDs5IntensityFull;

    return BuildDs5BluetoothFrameAs(layout, seq, effects, sizeof(effects), out, out_len);
}


inline std::size_t BuildDs5TriggerFrame(std::uint8_t seq,
                                        std::uint8_t left_type, const std::uint8_t* left_params,
                                        std::uint8_t right_type, const std::uint8_t* right_params,
                                        std::uint8_t* out, std::size_t out_len,
                                        std::uint8_t intensity = kDs5IntensityFull)
{
    if (left_params == nullptr || right_params == nullptr)
        return 0;

    std::uint8_t effects[kDs5EffectsBytes] = {};

    effects[Ds5Effects_EnableBits1] = Ds5Enable1_LeftTrigger | Ds5Enable1_RightTrigger;
    effects[Ds5Effects_EnableBits2] = Ds5Enable2_Intensity;
    effects[Ds5Effects_Intensity]   = intensity;

    effects[Ds5Effects_LeftTrigger] = left_type;
    std::memcpy(effects + Ds5Effects_LeftTrigger + 1, left_params, kDs5TriggerParamBytes);

    effects[Ds5Effects_RightTrigger] = right_type;
    std::memcpy(effects + Ds5Effects_RightTrigger + 1, right_params, kDs5TriggerParamBytes);

    return BuildDs5BluetoothFrame(seq, effects, sizeof(effects), out, out_len);
}

inline constexpr std::size_t kDs5HapticFrameBytes  = 547;
inline constexpr std::size_t kDs5HapticBlockBytes  = 64;

inline constexpr std::size_t kDs5HapticFrames      = kDs5HapticBlockBytes;

inline constexpr int kDs5HapticSampleRate = 3000;

inline constexpr std::uint8_t kDs5AudioBufferLength = 48;

inline std::size_t BuildDs5HapticFrame(std::uint8_t sequence, std::uint8_t packet_counter,
                                       const std::int8_t* samples, std::size_t sample_bytes,
                                       std::uint8_t* out, std::size_t out_len)
{
    if (samples == nullptr || out == nullptr)
        return 0;
    if (sample_bytes != kDs5HapticBlockBytes * 2 || out_len < kDs5HapticFrameBytes)
        return 0;

    std::memset(out, 0, kDs5HapticFrameBytes);

    out[0] = 0x39;
    out[1] = (std::uint8_t)((sequence & 0x0f) << 4);
    out[2] = 0x91;
    out[3] = 6;
    out[4] = 0x7e;

    out[5] = out[6] = out[7] = out[8] = kDs5AudioBufferLength;

    out[9]  = packet_counter;
    out[10] = 0xd2;
    out[11] = (std::uint8_t)kDs5HapticBlockBytes;

    std::memcpy(out + 12, samples, sample_bytes);

    StampDs5Crc(out, kDs5HapticFrameBytes);
    return kDs5HapticFrameBytes;
}

inline bool PadTakesDirectOutput(std::uint16_t vendor_id, std::uint16_t product_id)
{
    return vendor_id == 0x054c && (product_id == 0x0ce6 || product_id == 0x0df2);
}

} // namespace akira::platform::extended

#endif // AKIRA_INPUT_PS_OUTPUT_HPP
