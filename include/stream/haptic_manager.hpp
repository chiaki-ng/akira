#ifndef AKIRA_IO_HAPTIC_MANAGER_HPP
#define AKIRA_IO_HAPTIC_MANAGER_HPP

#include <cstdint>
#include <chrono>
#include <atomic>

#include "input/ds5_intensity.hpp"
#include "input/rumble_profile.hpp"
#include <memory>
#include <span>

#include "platform/pad_haptics.hpp"

class InputManager;

class HapticManager
{
public:
    explicit HapticManager(std::unique_ptr<akira::platform::PadHaptics> pad);
    ~HapticManager();

    // Disable copy
    HapticManager(const HapticManager&) = delete;
    HapticManager& operator=(const HapticManager&) = delete;


    void setRumble(uint8_t left, uint8_t right);
    void processHapticAudio(std::span<const std::byte> pcm);
    void cleanup();

    int hapticBase = 400;

    void setInputManager(InputManager* input) { m_input = input; }

    void setRumbleStrength(float strength) { m_rumble_strength = strength; }

    void setConsoleVibration(uint8_t wire)
    {
        m_console_vibration.store(wire, std::memory_order_relaxed);
    }
    void setRumbleSource(akira::input::RumbleSource source) { m_rumble_source = source; }
    void setHapticIntensity(akira::input::HapticIntensity intensity) { m_profile.haptic_intensity = intensity; }
    void setRumbleCeiling(float ceiling) { m_ceiling = ceiling; }

    void setRumbleFreqLow(float freq) { m_freq_low = freq; }
    void setRumbleFreqHigh(float freq) { m_freq_high = freq; }
    void setEnvelopeDecay(float decay) { m_envelope_decay = decay; }
    void setEnvelopeAttack(float attack) { m_envelope_attack = attack; }

    bool isLocked() const { return m_haptic_lock; }

    const akira::input::RumbleProfile& profile() const { return m_profile; }
    void setProfile(const akira::input::RumbleProfile& profile) { m_profile = profile; }
    float consoleVibrationScale() const;
    void emitHaptic(float left, float right);

private:
    void setHapticRumble(uint8_t left, uint8_t right);
    void cleanupHaptic();

    void emitMotorsLegacy(const uint8_t* buf, size_t buf_size);

    akira::input::RumbleProfile   m_profile;

    void emit(float left, float right);

    InputManager* m_input = nullptr;
    std::unique_ptr<akira::platform::PadHaptics> m_pad;
    float m_rumble_strength = 1.0f;

    std::atomic<uint8_t> m_console_vibration{
        (uint8_t)akira::input::Ds5EffectIntensity::Strong};
    float m_freq_low = 140.0f;
    float m_freq_high = 185.0f;
    float m_envelope = 0.0f;
    float m_envelope_decay = 0.85f;
    float m_envelope_attack = 0.60f;
    float m_ceiling = 1.0f;

    std::uint32_t m_rumble_events = 0;
    std::uint32_t m_haptic_buffers = 0;
    std::uint32_t m_stream_logged = 0;

    akira::input::RumbleSource m_rumble_source = akira::input::RumbleSource::Derived;

    akira::input::RumbleSource effectiveSource() const;




    bool m_haptic_lock = false;
    int m_haptic_val = 0;
    std::chrono::system_clock::time_point m_haptic_lock_time;
};

#endif // AKIRA_IO_HAPTIC_MANAGER_HPP
