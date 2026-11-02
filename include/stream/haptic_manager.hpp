#ifndef AKIRA_IO_HAPTIC_MANAGER_HPP
#define AKIRA_IO_HAPTIC_MANAGER_HPP

#include <cstdint>
#include <chrono>
#include <atomic>

#include "input/ps_output.hpp"
#include "input/rumble_profile.hpp"
#include <span>

class InputManager;
class ExtendedInputManager;

class HapticManager
{
public:
    HapticManager();
    ~HapticManager();

    // Disable copy
    HapticManager(const HapticManager&) = delete;
    HapticManager& operator=(const HapticManager&) = delete;


    void setRumble(uint8_t left, uint8_t right);
    void processHapticAudio(std::span<const std::byte> pcm);
    void cleanup();

    int hapticBase = 400;

    void setInputManager(InputManager* input) { m_input = input; }

    void setExtendedInput(ExtendedInputManager* extended) { m_extended = extended; }

    void setRumbleStrength(float strength) { m_rumble_strength = strength; }

    void setConsoleVibration(uint8_t wire)
    {
        m_console_vibration.store(wire, std::memory_order_relaxed);
    }
    void setRumbleSource(akira::input::RumbleSource source) { m_rumble_source = source; }
    void setRumbleCeiling(float ceiling) { m_ceiling = ceiling; }

    void setRumbleFreqLow(float freq) { m_freq_low = freq; }
    void setRumbleFreqHigh(float freq) { m_freq_high = freq; }
    void setEnvelopeDecay(float decay) { m_envelope_decay = decay; }
    void setEnvelopeAttack(float attack) { m_envelope_attack = attack; }

    bool isLocked() const { return m_haptic_lock; }

    void refreshProfile();

private:
    void setHapticRumble(uint8_t left, uint8_t right);
    void cleanupHaptic();

    bool nativeRumble() const;

    void emitMotorsFromWaveform(const int16_t* stereo, size_t frames);
    void emitMotorsLegacy(const uint8_t* buf, size_t buf_size);

    bool streamNativeHaptics(const int16_t* stereo, size_t frames);

    static constexpr float kNativeRelease = 0.75f;

    static constexpr float kNativeSilence = 2.0f / 255.0f;

    akira::input::RumbleProfile   m_profile;
    uint64_t                      m_profile_generation = 0;
    float    m_native_env_left  = 0.0f;
    float    m_native_env_right = 0.0f;

    int8_t   m_haptic_block[akira::input::kDs5HapticBlockBytes * 2]{};
    size_t   m_haptic_fill    = 0;
    uint8_t  m_haptic_counter = 0;
    uint32_t m_haptic_sent    = 0;
    uint32_t m_haptic_skipped = 0;
    bool     m_haptic_stopped_logged = false;

    std::chrono::steady_clock::time_point m_haptic_last_send{};
    bool m_haptic_last_send_valid = false;
    std::chrono::steady_clock::time_point m_haptic_first_send{};
    bool m_haptic_first_send_valid = false;
    bool     m_haptic_logged  = false;



    static constexpr uint32_t kNativeHapticLogInterval = 60;
    uint32_t m_native_logged    = 0;



    void emit(float left, float right);

    InputManager* m_input = nullptr;
    ExtendedInputManager* m_extended = nullptr;
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
