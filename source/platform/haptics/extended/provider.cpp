#include "platform/haptics/extended/provider.hpp"
#include "platform/pad_settings/extended/provider.hpp"
#include "log/log.hpp"

#include "core/settings_manager.hpp"
#include "platform/pads/extended/rumble_keys.hpp"
#include "stream/haptic_manager.hpp"
#include "platform/pads/extended/provider.hpp"

namespace akira::platform::extended
{

ExtendedHapticsProvider::ExtendedHapticsProvider(ExtendedPadProvider& pads)
    : m_pads(pads)
{
}

input::RumbleSource ExtendedHapticsProvider::effectiveSource(input::RumbleSource configured) const
{
    if (configured == input::RumbleSource::Off)
        return input::RumbleSource::Off;
    if (tunedForPad())
        return input::RumbleSource::Derived;
    return configured;
}

void ExtendedHapticsProvider::prepare(HapticManager& haptics)
{
    const PadPathInfo path = m_pads.pathInfo();
    if (path.generation == m_profile_generation)
        return;
    m_profile_generation = path.generation;
    if (!path.available)
        return;

    haptics.setProfile(akira::platform::extended::ExtendedPadSettingsProvider::resolveProfile(*SettingsManager::getInstance(), 
        path.vendorId, path.productId, path.hasAddress ? path.address.data() : nullptr,
        path.switchNative, path.kind == input::PadPathKind::JoyCon));
}

bool ExtendedHapticsProvider::play(HapticManager& haptics, std::span<const std::int16_t> stereo)
{
    if (nativeRumble() && streamNativeHaptics(haptics, stereo))
        return true;

    if (!tunedForPad())
        return false;

    emitMotorsFromWaveform(haptics, stereo);
    return true;
}

bool ExtendedHapticsProvider::nativeRumble() const
{
    const PadPathInfo path = m_pads.pathInfo();
    return path.available && path.nativeRumble;
}

bool ExtendedHapticsProvider::tunedForPad() const
{
    const PadPathInfo path = m_pads.pathInfo();
    return path.available && PadTakesDirectOutput(path.vendorId, path.productId);
}

bool ExtendedHapticsProvider::streamNativeHaptics(HapticManager& haptics, std::span<const std::int16_t> stereo)
{
    ExtendedInputManager& extended = m_pads.extendedInput();
    if (!extended.directHapticsReady()) {
        if (!m_haptic_stopped_logged && m_haptic_sent != 0) {
            m_haptic_stopped_logged = true;
            akira::log::info("haptic stream: stopped after {} frames - handing the"
                               " signal back to the motors", m_haptic_sent);
        }
        return false;
    }

    const float gain = input::HapticSampleGain(haptics.profile().haptic_intensity)
                     * haptics.consoleVibrationScale();

    for (const std::int16_t sample : stereo) {
        std::int32_t v = (std::int32_t)((float)(sample >> 8) * gain);
        if (v >  127) v =  127;
        if (v < -128) v = -128;

        m_haptic_block[m_haptic_fill++] = (std::int8_t)v;

        if (m_haptic_fill < kDs5HapticBlockBytes * 2)
            continue;

        m_haptic_fill = 0;

        int block_peak = 0;
        for (std::size_t k = 0; k < kDs5HapticBlockBytes * 2; k++) {
            const int a = m_haptic_block[k] < 0 ? -(int)m_haptic_block[k]
                                                :  (int)m_haptic_block[k];
            if (a > block_peak)
                block_peak = a;
        }
        if (block_peak <= kHapticBlockFloor) {
            m_haptic_skipped++;
            continue;
        }

        m_haptic_last_send = std::chrono::steady_clock::now();

        std::uint8_t frame[kDs5HapticFrameBytes];
        const std::size_t len = BuildDs5HapticFrame(
            0, m_haptic_counter,
            m_haptic_block, kDs5HapticBlockBytes * 2,
            frame, sizeof(frame));

        m_haptic_counter = (std::uint8_t)(m_haptic_counter + 2);

        if (len == 0)
            continue;

        if (!extended.sendDirectHaptics(frame, (std::uint16_t)len)) {
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
            akira::log::info("haptic stream: {} sent, {} skipped,"
                               " {} ms elapsed, {} per sec",
                               m_haptic_sent, m_haptic_skipped,
                               (long long)ms,
                               ms > 0 ? (long long)(m_haptic_sent * 1000ll / ms) : 0ll);
        }

        if (!m_haptic_logged) {
            m_haptic_logged = true;
            akira::log::info("haptic stream: writing to the pad's actuators ({} bytes)", len);
        }
    }

    return true;
}

void ExtendedHapticsProvider::emitMotorsFromWaveform(HapticManager& haptics, std::span<const std::int16_t> stereo)
{
    const HapticRumble r =
        HapticAudioToRumble(stereo.data(), stereo.size() / 2, haptics.profile().haptic_intensity,
                                   !tunedForPad());

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
            akira::log::info("haptic->rumble: raw={}/{} scaled={}/{} out={}/{}{}",
                               r.raw_left, r.raw_right, r.left, r.right,
                               (int)(m_native_env_left * 255.0f),
                               (int)(m_native_env_right * 255.0f),
                               (r.left >= 0xffff || r.right >= 0xffff) ? " SATURATED" : "");
        }
    }

    haptics.emitHaptic(m_native_env_left, m_native_env_right);
}

}
