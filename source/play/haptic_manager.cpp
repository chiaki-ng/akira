#include "play/haptic_manager.hpp"
#include "log/log.hpp"

#include <cstdio>
#include "play/input_manager.hpp"
#include "settings/settings_manager.hpp"
#include "play/ds5_intensity.hpp"
#include "play/rumble_profile.hpp"
#include <algorithm>
#include <cstring>

namespace akira::play {

HapticManager::HapticManager(std::unique_ptr<akira::platform::HapticsProvider> pad)
    : m_pad(std::move(pad))
{
}

HapticManager::~HapticManager()
{
}


RumbleSource HapticManager::effectiveSource() const
{
    return m_pad->effectiveSource(m_rumble_source);
}

float HapticManager::consoleVibrationScale() const
{
    return Ds5IntensityScale(
        Ds5IntensityFromWire(m_console_vibration.load(std::memory_order_relaxed)));
}

void HapticManager::emitHaptic(float left, float right)
{
    m_haptic_val = (int)((left > right ? left : right) * 255.0f);
    m_haptic_lock_time = std::chrono::high_resolution_clock::now();
    m_haptic_lock = m_haptic_val != 0;

    emit(left, right);
}

void HapticManager::emit(float left, float right)
{
    if (!m_input)
        return;

    const float nonNativeScale = consoleVibrationScale();

    if (left  > m_ceiling) left  = m_ceiling;
    if (right > m_ceiling) right = m_ceiling;

    m_input->sendRumble(left, right, m_freq_low, m_freq_high, nonNativeScale);
}

void HapticManager::setRumble(uint8_t left, uint8_t right)
{
    if (SettingsManager::getInstance()->getDebugChiakiLog() && (left > 0 || right > 0))
        log::info("setRumble: left={}, right={}, strength={:.2f}", left, right, m_rumble_strength);

    m_rumble_events++;
    if (m_rumble_events == 1)
        log::info("rumble events: console sent one while announced as a DualSense"
                           " (haptic buffers so far: {})", m_haptic_buffers);

    if (effectiveSource() != RumbleSource::Game)
        return;

    float leftAmp  = (left  > 0) ? ((float)left  / 255.0f) * m_rumble_strength : 0.0f;
    float rightAmp = (right > 0) ? ((float)right / 255.0f) * m_rumble_strength : 0.0f;

    emit(leftAmp, rightAmp);
}

void HapticManager::emitMotorsLegacy(const uint8_t* buf, size_t buf_size)
{
    int16_t amplitudel = 0, amplituder = 0;
    int32_t suml = 0, sumr = 0;
    const size_t sample_size = 2 * sizeof(int16_t);

    size_t buf_count = buf_size / sample_size;
    if (buf_count == 0)
        return;

    for (size_t i = 0; i < buf_count; i++)
    {
        size_t cur = i * sample_size;
        memcpy(&amplitudel, buf + cur, sizeof(int16_t));
        memcpy(&amplituder, buf + cur + sizeof(int16_t), sizeof(int16_t));
        suml += (amplitudel < 0) ? -amplitudel : amplitudel;
        sumr += (amplituder < 0) ? -amplituder : amplituder;
    }

    int32_t avgLeft = suml / static_cast<int32_t>(buf_count) / 64;
    int32_t avgRight = sumr / static_cast<int32_t>(buf_count) / 64;
    uint8_t left = static_cast<uint8_t>(avgLeft > 255 ? 255 : avgLeft);
    uint8_t right = static_cast<uint8_t>(avgRight > 255 ? 255 : avgRight);

    setHapticRumble(left, right);
    if ((left != 0 || right != 0) && !m_haptic_lock)
    {
        m_haptic_lock = true;
    }
}

void HapticManager::processHapticAudio(std::span<const std::byte> pcm)
{
    const auto* buf = reinterpret_cast<const uint8_t*>(pcm.data());
    const size_t buf_size = pcm.size();
    const std::span<const int16_t> stereo(reinterpret_cast<const int16_t*>(buf),
                                          buf_size / (2 * sizeof(int16_t)) * 2);

    m_pad->prepare(*this);

    m_haptic_buffers++;
    if (m_haptic_buffers == 1)
        log::info("haptic stream: first buffer (rumble events so far: {})",
                           m_rumble_events);
    if (++m_stream_logged >= 600) {
        m_stream_logged = 0;
        log::info("haptic stream: {} buffers, {} rumble events, source={}",
                           m_haptic_buffers, m_rumble_events, (int)effectiveSource());
    }

    if (effectiveSource() != RumbleSource::Derived)
        return;

    if (Ds5IntensityFromWire(m_console_vibration.load(std::memory_order_relaxed))
        == Ds5EffectIntensity::Off)
        return;

    if (m_pad->play(*this, stereo))
        return;

    emitMotorsLegacy(buf, buf_size);
}

void HapticManager::setHapticRumble(uint8_t left, uint8_t right)
{
    uint8_t val = left > right ? left : right;
    m_haptic_val = val;
    m_haptic_lock_time = std::chrono::high_resolution_clock::now();


    if (m_profile.haptic_intensity == HapticIntensity::Off) {
        m_envelope = 0.0f;
        emit(0.0f, 0.0f);
        return;
    }

    float amplitude = (float)val / (float)hapticBase;
    if (amplitude > 1.0f) amplitude = 1.0f;
    amplitude *= m_rumble_strength;

    if (amplitude >= m_envelope)
        m_envelope = m_envelope * (1.0f - m_envelope_attack) + amplitude * m_envelope_attack;
    else
        m_envelope = m_envelope * m_envelope_decay + amplitude * (1.0f - m_envelope_decay);

    emit(m_envelope, m_envelope);
}

void HapticManager::cleanupHaptic()
{
    std::chrono::system_clock::time_point now = std::chrono::high_resolution_clock::now();
    auto dur = now - m_haptic_lock_time;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
    if (m_haptic_val == 0)
    {
        m_haptic_lock = false;
    }
    else if (ms > 30)
    {
        m_envelope = 0.0f;
        m_haptic_val = 0;
        m_haptic_lock = false;
        emit(0.0f, 0.0f);
    }
}

void HapticManager::cleanup()
{
    m_envelope = 0.0f;
    m_haptic_lock = false;
    m_haptic_val = 0;
    emit(0.0f, 0.0f);
}

}
