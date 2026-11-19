#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "platform/pads/extended/ps_output.hpp"
#include "platform/haptics/provider.hpp"


namespace akira::play
{
class ControllerSettings;
}

namespace akira::settings
{
class DeveloperSettings;
}

namespace akira::platform::extended
{

class ExtendedPadProvider;

class ExtendedHapticsProvider final : public HapticsProvider
{
public:
    ExtendedHapticsProvider(ExtendedPadProvider& pads, const play::ControllerSettings& controllers,
        const settings::DeveloperSettings& developer);

    play::RumbleSource effectiveSource(play::RumbleSource configured) const override;
    void prepare(play::HapticManager& haptics) override;
    bool play(play::HapticManager& haptics, std::span<const std::int16_t> stereo) override;

private:
    bool nativeRumble() const;
    bool tunedForPad() const;
    bool streamNativeHaptics(play::HapticManager& haptics, std::span<const std::int16_t> stereo);
    void emitMotorsFromWaveform(play::HapticManager& haptics, std::span<const std::int16_t> stereo);

    static constexpr float kNativeRelease = 0.75f;
    static constexpr float kNativeSilence = 2.0f / 255.0f;
    static constexpr std::uint32_t kNativeHapticLogInterval = 60;

    ExtendedPadProvider& m_pads;
    const play::ControllerSettings& m_controllers;
    const settings::DeveloperSettings& m_developer;

    std::uint64_t m_profile_generation = 0;
    float m_native_env_left = 0.0f;
    float m_native_env_right = 0.0f;

    std::int8_t m_haptic_block[kDs5HapticBlockBytes * 2]{};
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
