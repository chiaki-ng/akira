#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "platform/nx/ps_output.hpp"
#include "platform/pad_haptics.hpp"


namespace akira::platform
{

class NxPadSource;

class NxHaptics final : public PadHaptics
{
public:
    explicit NxHaptics(NxPadSource& pads);

    input::RumbleSource effectiveSource(input::RumbleSource configured) const override;
    void prepare(HapticManager& haptics) override;
    bool play(HapticManager& haptics, std::span<const std::int16_t> stereo) override;

private:
    bool nativeRumble() const;
    bool tunedForPad() const;
    bool streamNativeHaptics(HapticManager& haptics, std::span<const std::int16_t> stereo);
    void emitMotorsFromWaveform(HapticManager& haptics, std::span<const std::int16_t> stereo);

    static constexpr float kNativeRelease = 0.75f;
    static constexpr float kNativeSilence = 2.0f / 255.0f;
    static constexpr std::uint32_t kNativeHapticLogInterval = 60;

    NxPadSource& m_pads;

    std::uint64_t m_profile_generation = 0;
    float m_native_env_left = 0.0f;
    float m_native_env_right = 0.0f;

    std::int8_t m_haptic_block[input::kDs5HapticBlockBytes * 2]{};
    std::size_t m_haptic_fill = 0;
    std::uint8_t m_haptic_counter = 0;
    std::uint32_t m_haptic_sent = 0;
    std::uint32_t m_haptic_skipped = 0;
    bool m_haptic_stopped_logged = false;

    std::chrono::steady_clock::time_point m_haptic_last_send{};
    std::chrono::steady_clock::time_point m_haptic_first_send{};
    bool m_haptic_first_send_valid = false;
    bool m_haptic_logged = false;

    std::uint32_t m_native_logged = 0;
};

}
