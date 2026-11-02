#include "stream/haptic_manager.hpp"

#include <cstdio>
#include "stream/input_manager.hpp"
#include "core/settings_manager.hpp"
#include "input/ps_output.hpp"
#include "input/rumble_profile.hpp"
#include <algorithm>
#include <cstring>
#include <borealis.hpp>

HapticManager::HapticManager()
{
}

HapticManager::~HapticManager()
{
}


akira::input::RumbleSource HapticManager::effectiveSource() const
{
    if (m_rumble_source == akira::input::RumbleSource::Off)
        return akira::input::RumbleSource::Off;

    const PadPathInfo path = m_input ? m_input->pathInfo() : PadPathInfo{};
    if (path.available
        && akira::input::PadTakesDirectOutput(path.vendorId, path.productId))
        return akira::input::RumbleSource::Derived;

    return m_rumble_source;
}

void HapticManager::emit(float left, float right)
{
    if (!m_input)
        return;

    const float nonNativeScale = akira::input::Ds5IntensityScale(
        akira::input::Ds5IntensityFromWire(
            m_console_vibration.load(std::memory_order_relaxed)));

    if (left  > m_ceiling) left  = m_ceiling;
    if (right > m_ceiling) right = m_ceiling;

    m_input->sendRumble(left, right, m_freq_low, m_freq_high, nonNativeScale);
}

void HapticManager::setRumble(uint8_t left, uint8_t right)
{
    if (SettingsManager::getInstance()->getDebugChiakiLog() && (left > 0 || right > 0))
        brls::Logger::info("setRumble: left={}, right={}, strength={:.2f}", left, right, m_rumble_strength);

    m_rumble_events++;
    if (m_rumble_events == 1)
        brls::Logger::info("rumble events: console sent one while announced as a DualSense"
                           " (haptic buffers so far: {})", m_haptic_buffers);

    if (effectiveSource() != akira::input::RumbleSource::Game)
        return;

    float leftAmp  = (left  > 0) ? ((float)left  / 255.0f) * m_rumble_strength : 0.0f;
    float rightAmp = (right > 0) ? ((float)right / 255.0f) * m_rumble_strength : 0.0f;

    emit(leftAmp, rightAmp);
}

void HapticManager::refreshProfile()
{
    const PadPathInfo path = m_input ? m_input->pathInfo() : PadPathInfo{};
    if (!path.available) {
        m_profile_generation = path.generation;
        return;
    }

    m_profile = SettingsManager::getInstance()->resolveRumbleProfile(
        path.vendorId, path.productId, path.hasAddress ? path.address.data() : nullptr,
        path.switchNative, path.kind == akira::input::PadPathKind::JoyCon);
    m_profile_generation = path.generation;
}

bool HapticManager::streamNativeHaptics(const int16_t* stereo, size_t frames)
{
    if (!m_extended || !m_extended->directHapticsReady()) {
        if (!m_haptic_stopped_logged && m_haptic_sent != 0) {
            m_haptic_stopped_logged = true;
            brls::Logger::info("haptic stream: stopped after {} frames - handing the"
                               " signal back to the motors", m_haptic_sent);
        }
        return false;
    }

    const float gain = akira::input::HapticSampleGain(m_profile.haptic_intensity)
                     * akira::input::Ds5IntensityScale(
                           akira::input::Ds5IntensityFromWire(
                               m_console_vibration.load(std::memory_order_relaxed)));

    for (size_t i = 0; i < frames * 2; i++) {
        int32_t v = (int32_t)((float)(stereo[i] >> 8) * gain);
        if (v >  127) v =  127;
        if (v < -128) v = -128;

        m_haptic_block[m_haptic_fill++] = (int8_t)v;

        if (m_haptic_fill < akira::input::kDs5HapticBlockBytes * 2)
            continue;

        m_haptic_fill = 0;

        int block_peak = 0;
        for (std::size_t k = 0; k < akira::input::kDs5HapticBlockBytes * 2; k++) {
            const int a = m_haptic_block[k] < 0 ? -(int)m_haptic_block[k]
                                                :  (int)m_haptic_block[k];
            if (a > block_peak)
                block_peak = a;
        }
        if (block_peak <= akira::input::kHapticBlockFloor) {
            m_haptic_skipped++;
            continue;
        }

        m_haptic_last_send = std::chrono::steady_clock::now();
        m_haptic_last_send_valid = true;

        uint8_t frame[akira::input::kDs5HapticFrameBytes];
        const size_t len = akira::input::BuildDs5HapticFrame(
            0, m_haptic_counter,
            m_haptic_block, akira::input::kDs5HapticBlockBytes * 2,
            frame, sizeof(frame));

        m_haptic_counter  = (uint8_t)(m_haptic_counter + 2);

        if (len == 0)
            continue;

        if (!m_extended->sendDirectHaptics(frame, (uint16_t)len)) {
            m_haptic_fill = 0;
            return false;
        }

        m_haptic_sent++;

        if ((m_haptic_sent % 25) == 0) {
            if (!m_haptic_first_send_valid) {
                m_haptic_first_send = m_haptic_last_send;
                m_haptic_first_send_valid = true;
            }
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                m_haptic_last_send - m_haptic_first_send).count();
            brls::Logger::info("haptic stream: {} sent, {} skipped,"
                               " {} ms elapsed, {} per sec",
                               m_haptic_sent, m_haptic_skipped,
                               (long long)ms,
                               ms > 0 ? (long long)(m_haptic_sent * 1000ll / ms) : 0ll);
        }

        if (!m_haptic_logged) {
            m_haptic_logged = true;
            brls::Logger::info("haptic stream: writing to the pad's actuators ({} bytes)", len);
        }
    }

    return true;
}

void HapticManager::emitMotorsFromWaveform(const int16_t* stereo, size_t frames)
{
    const PadPathInfo path = m_input ? m_input->pathInfo() : PadPathInfo{};
    const bool motor_stalls =
        !path.available
        || !akira::input::PadTakesDirectOutput(path.vendorId, path.productId);

    const akira::input::HapticRumble r =
        akira::input::HapticAudioToRumble(stereo, frames, m_profile.haptic_intensity,
                                          motor_stalls);

    const float target_left  = r.emit ? (float)r.left  / 65535.0f : 0.0f;
    const float target_right = r.emit ? (float)r.right / 65535.0f : 0.0f;

    m_native_env_left  = target_left  > m_native_env_left
                             ? target_left
                             : m_native_env_left  * kNativeRelease + target_left  * (1.0f - kNativeRelease);
    m_native_env_right = target_right > m_native_env_right
                             ? target_right
                             : m_native_env_right * kNativeRelease + target_right * (1.0f - kNativeRelease);

    if (m_native_env_left  < kNativeSilence) m_native_env_left  = 0.0f;
    if (m_native_env_right < kNativeSilence) m_native_env_right = 0.0f;

    if (++m_native_logged >= kNativeHapticLogInterval) {
        m_native_logged = 0;
        if (SettingsManager::getInstance()->getDebugChiakiLog()) {
            brls::Logger::info("haptic->rumble: raw={}/{} scaled={}/{} out={}/{}{}",
                               r.raw_left, r.raw_right, r.left, r.right,
                               (int)(m_native_env_left * 255.0f),
                               (int)(m_native_env_right * 255.0f),
                               (r.left >= 0xffff || r.right >= 0xffff) ? " SATURATED" : "");
        }
    }

    m_haptic_val = (int)((m_native_env_left > m_native_env_right
                              ? m_native_env_left : m_native_env_right) * 255.0f);
    m_haptic_lock_time = std::chrono::high_resolution_clock::now();
    m_haptic_lock = m_haptic_val != 0;

    emit(m_native_env_left, m_native_env_right);
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
    const auto* stereo = reinterpret_cast<const int16_t*>(buf);
    const size_t frames = buf_size / (2 * sizeof(int16_t));

    if (m_input) {
        const PadPathInfo path = m_input->pathInfo();
        if (path.generation != m_profile_generation)
            refreshProfile();
    }

    m_haptic_buffers++;
    if (m_haptic_buffers == 1)
        brls::Logger::info("haptic stream: first buffer (rumble events so far: {})",
                           m_rumble_events);
    if (++m_stream_logged >= 600) {
        m_stream_logged = 0;
        brls::Logger::info("haptic stream: {} buffers, {} rumble events, source={}",
                           m_haptic_buffers, m_rumble_events, (int)effectiveSource());
    }

    if (effectiveSource() != akira::input::RumbleSource::Derived)
        return;

    if (akira::input::Ds5IntensityFromWire(m_console_vibration.load(std::memory_order_relaxed))
        == akira::input::Ds5EffectIntensity::Off)
        return;

    if (nativeRumble() && streamNativeHaptics(stereo, frames))
        return;

    const PadPathInfo path = m_input ? m_input->pathInfo() : PadPathInfo{};
    const bool tuned_for_this_pad =
        path.available
        && akira::input::PadTakesDirectOutput(path.vendorId, path.productId);

    if (tuned_for_this_pad)
        emitMotorsFromWaveform(stereo, frames);
    else
        emitMotorsLegacy(buf, buf_size);
}

bool HapticManager::nativeRumble() const
{
    if (!m_input)
        return false;

    const PadPathInfo path = m_input->pathInfo();
    return path.available && path.nativeRumble;
}

void HapticManager::setHapticRumble(uint8_t left, uint8_t right)
{
    uint8_t val = left > right ? left : right;
    m_haptic_val = val;
    m_haptic_lock_time = std::chrono::high_resolution_clock::now();


    if (m_profile.haptic_intensity == akira::input::HapticIntensity::Off) {
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
