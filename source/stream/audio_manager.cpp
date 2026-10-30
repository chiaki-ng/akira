#include "stream/audio_manager.hpp"
#include <borealis.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

namespace {

constexpr unsigned int kPrefillMs = 30;
constexpr unsigned int kLowWatermarkMs = 20;
constexpr unsigned int kTargetWatermarkMs = 30;
constexpr unsigned int kHighWatermarkMs = 50;
constexpr unsigned int kCapacityMs = 60;
constexpr unsigned int kControlBlockMs = 10;
constexpr Uint16 kSwitchOutputFrames = 1024;

std::uint64_t steadySeconds()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

std::uint64_t steadyMicroseconds()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

template <typename T>
void updateAtomicMin(std::atomic<T>& target, T value)
{
    auto observed = target.load(std::memory_order_relaxed);
    while (value < observed &&
           !target.compare_exchange_weak(
               observed,
               value,
               std::memory_order_relaxed))
    {
    }
}

template <typename T>
void updateAtomicMax(std::atomic<T>& target, T value)
{
    auto observed = target.load(std::memory_order_relaxed);
    while (value > observed &&
           !target.compare_exchange_weak(
               observed,
               value,
               std::memory_order_relaxed))
    {
    }
}

} // namespace

AudioManager::AudioManager()
{
}

AudioManager::~AudioManager()
{
    cleanup();
}

bool AudioManager::init(unsigned int channels, unsigned int rate)
{
    cleanup();

    if (channels == 0 || rate == 0 ||
        !m_ring.configure(
            rate,
            channels,
            kPrefillMs,
            kLowWatermarkMs,
            kTargetWatermarkMs,
            kHighWatermarkMs,
            kCapacityMs,
            kControlBlockMs))
    {
        m_open_errors.fetch_add(1, std::memory_order_relaxed);
        brls::Logger::error(
            "Invalid audio ring configuration: rate={}, channels={}",
            rate,
            channels);
        return false;
    }

    SDL_AudioSpec want;
    SDL_AudioSpec have;
    SDL_memset(&want, 0, sizeof(want));
    SDL_memset(&have, 0, sizeof(have));

    want.freq = rate;
    want.format = AUDIO_S16SYS;
    want.channels = channels;
    // The Switch SDL2 audren backend is unstable with small, non-power-of-two
    // wave buffers. Chiaki's established Switch path uses 1024 frames.
    want.samples = kSwitchOutputFrames;
    want.callback = &AudioManager::sdlAudioCallback;
    want.userdata = this;

    resetDiagnostics();
    m_shutdown.store(false, std::memory_order_release);
    m_callback_active.store(false, std::memory_order_release);
    m_playback_started.store(false, std::memory_order_release);
    m_last_summary_second.store(
        steadySeconds(),
        std::memory_order_relaxed);
    m_ring.notePrefillStart();

    m_device_id = SDL_OpenAudioDevice(
        nullptr,
        0,
        &want,
        &have,
        0);
    if (m_device_id == 0)
    {
        m_open_errors.fetch_add(1, std::memory_order_relaxed);
        brls::Logger::error(
            "SDL_OpenAudioDevice failed: {}",
            SDL_GetError());
        return false;
    }

    m_requested = want;
    m_obtained = have;
    if (have.freq != want.freq || have.format != want.format ||
        have.channels != want.channels)
    {
        m_open_errors.fetch_add(1, std::memory_order_relaxed);
        brls::Logger::error(
            "SDL_OpenAudioDevice changed PCM contract: requested "
            "freq={} channels={} format={} samples={}; obtained "
            "freq={} channels={} format={} samples={}",
            want.freq,
            want.channels,
            static_cast<int>(want.format),
            want.samples,
            have.freq,
            have.channels,
            static_cast<int>(have.format),
            have.samples);
        SDL_CloseAudioDevice(m_device_id);
        m_device_id = 0;
        return false;
    }

    brls::Logger::info(
        "Audio SDL open: requested_freq={} requested_channels={} "
        "requested_format={} requested_samples={} obtained_freq={} "
        "obtained_channels={} obtained_format={} obtained_samples={}",
        want.freq,
        want.channels,
        static_cast<int>(want.format),
        want.samples,
        have.freq,
        have.channels,
        static_cast<int>(have.format),
        have.samples);

    SDL_PauseAudioDevice(m_device_id, 0);
    return true;
}

void AudioManager::play(int16_t* buf, size_t samples_count)
{
    if (!isInitialized() || !buf || samples_count == 0)
    {
        return;
    }

    const auto channels = m_ring.channels();
    if (channels == 0 ||
        samples_count >
            std::numeric_limits<std::size_t>::max() / channels)
    {
        return;
    }

    const std::size_t sample_count = samples_count * channels;
    std::int64_t sample_sum = 0;
    std::uint64_t square_sum = 0;
    std::uint64_t clipped_samples = 0;
    std::uint64_t zero_samples = 0;
    std::uint64_t abs_peak = 0;
    for (std::size_t index = 0; index < sample_count; ++index)
    {
        const auto sample = static_cast<std::int32_t>(buf[index]);
        const auto magnitude = static_cast<std::uint64_t>(
            sample < 0 ? -sample : sample);
        sample_sum += sample;
        square_sum += static_cast<std::uint64_t>(
            static_cast<std::int64_t>(sample) * sample);
        clipped_samples +=
            sample == std::numeric_limits<std::int16_t>::min() ||
                    sample == std::numeric_limits<std::int16_t>::max()
                ? 1
                : 0;
        zero_samples += sample == 0 ? 1 : 0;
        abs_peak = std::max(abs_peak, magnitude);
    }
    m_pcm_sample_count.fetch_add(sample_count, std::memory_order_relaxed);
    m_pcm_sample_sum.fetch_add(sample_sum, std::memory_order_relaxed);
    m_pcm_square_sum.fetch_add(square_sum, std::memory_order_relaxed);
    m_pcm_clipped_samples.fetch_add(
        clipped_samples,
        std::memory_order_relaxed);
    m_pcm_zero_samples.fetch_add(zero_samples, std::memory_order_relaxed);
    updateAtomicMax(m_pcm_abs_peak, abs_peak);
    updateAtomicMin(
        m_pcm_min_block_frames,
        static_cast<std::uint64_t>(samples_count));
    updateAtomicMax(
        m_pcm_max_block_frames,
        static_cast<std::uint64_t>(samples_count));

    const auto lock_start_us = steadyMicroseconds();
    SDL_LockAudioDevice(m_device_id);
    const auto lock_wait_us = steadyMicroseconds() - lock_start_us;
    m_producer_lock_count.fetch_add(1, std::memory_order_relaxed);
    m_producer_lock_wait_total_us.fetch_add(
        lock_wait_us,
        std::memory_order_relaxed);
    updateAtomicMax(m_producer_lock_wait_max_us, lock_wait_us);
    if (m_shutdown.load(std::memory_order_acquire))
    {
        SDL_UnlockAudioDevice(m_device_id);
        return;
    }

    m_ring.producerWrite(buf, samples_count);
    const auto queued = m_ring.currentFrames();
    bool prefill_completed = false;
    if (!m_playback_started.load(std::memory_order_acquire) &&
        queued >= prefillFrames())
    {
        m_playback_started.store(true, std::memory_order_release);
        m_ring.notePrefillComplete();
        prefill_completed = true;
    }
    SDL_UnlockAudioDevice(m_device_id);

    if (prefill_completed)
    {
        brls::Logger::info(
            "Audio prefill complete at {} frames, starting SDL playback",
            queued);
    }

    logSummary(false);
}

void AudioManager::cleanup()
{
    if (m_device_id > 0)
    {
        m_shutdown.store(true, std::memory_order_release);
        SDL_PauseAudioDevice(m_device_id, 1);
        logSummary(true);
        SDL_CloseAudioDevice(m_device_id);
        m_device_id = 0;
        resetRing();
    }
}

std::size_t AudioManager::prefillFrames() const
{
    return static_cast<std::size_t>(m_ring.rate()) *
           kPrefillMs / 1000;
}

void AudioManager::resetRing()
{
    m_playback_started.store(false, std::memory_order_release);
    m_ring.clear();
}

void AudioManager::resetDiagnostics()
{
    m_last_callback_us.store(0, std::memory_order_relaxed);
    m_callback_frames.store(0, std::memory_order_relaxed);
    m_callback_started_frames.store(0, std::memory_order_relaxed);
    m_callback_interval_count.store(0, std::memory_order_relaxed);
    m_callback_interval_total_us.store(0, std::memory_order_relaxed);
    m_callback_interval_min_us.store(
        std::numeric_limits<std::uint64_t>::max(),
        std::memory_order_relaxed);
    m_callback_interval_max_us.store(0, std::memory_order_relaxed);
    m_callback_bad_lengths.store(0, std::memory_order_relaxed);
    m_producer_lock_count.store(0, std::memory_order_relaxed);
    m_producer_lock_wait_total_us.store(0, std::memory_order_relaxed);
    m_producer_lock_wait_max_us.store(0, std::memory_order_relaxed);
    m_pcm_sample_count.store(0, std::memory_order_relaxed);
    m_pcm_sample_sum.store(0, std::memory_order_relaxed);
    m_pcm_square_sum.store(0, std::memory_order_relaxed);
    m_pcm_clipped_samples.store(0, std::memory_order_relaxed);
    m_pcm_zero_samples.store(0, std::memory_order_relaxed);
    m_pcm_abs_peak.store(0, std::memory_order_relaxed);
    m_pcm_min_block_frames.store(
        std::numeric_limits<std::uint64_t>::max(),
        std::memory_order_relaxed);
    m_pcm_max_block_frames.store(0, std::memory_order_relaxed);
}

void AudioManager::sdlAudioCallback(
    void* userdata,
    Uint8* stream,
    int len)
{
    auto* self = static_cast<AudioManager*>(userdata);
    if (self)
    {
        self->audioCallback(stream, len);
    }
    else if (stream && len > 0)
    {
        std::memset(stream, 0, static_cast<std::size_t>(len));
    }
}

void AudioManager::audioCallback(Uint8* stream, int len)
{
    if (!stream || len <= 0 || !m_ring.configured() ||
        m_shutdown.load(std::memory_order_acquire))
    {
        if (stream && len > 0)
        {
            std::memset(stream, 0, static_cast<std::size_t>(len));
        }
        return;
    }

    m_callback_active.store(true, std::memory_order_release);
    const auto bytes_per_frame =
        m_ring.channels() * sizeof(int16_t);
    const std::size_t frames =
        bytes_per_frame == 0
            ? 0
            : static_cast<std::size_t>(len) / bytes_per_frame;
    if (bytes_per_frame == 0 ||
        static_cast<std::size_t>(len) % bytes_per_frame != 0)
    {
        m_callback_bad_lengths.fetch_add(1, std::memory_order_relaxed);
    }
    m_callback_frames.fetch_add(frames, std::memory_order_relaxed);

    const auto callback_us = steadyMicroseconds();
    const auto previous_callback_us =
        m_last_callback_us.exchange(callback_us, std::memory_order_relaxed);
    if (previous_callback_us > 0 && callback_us >= previous_callback_us)
    {
        const auto interval_us = callback_us - previous_callback_us;
        m_callback_interval_count.fetch_add(1, std::memory_order_relaxed);
        m_callback_interval_total_us.fetch_add(
            interval_us,
            std::memory_order_relaxed);
        updateAtomicMin(m_callback_interval_min_us, interval_us);
        updateAtomicMax(m_callback_interval_max_us, interval_us);
    }

    const auto started =
        m_playback_started.load(std::memory_order_acquire);
    if (started)
    {
        m_callback_started_frames.fetch_add(
            frames,
            std::memory_order_relaxed);
    }
    const auto before = m_ring.currentFrames();
    const std::size_t missing =
        started && before < frames ? frames - before : 0;

    m_ring.consumerRead(
        reinterpret_cast<int16_t*>(stream),
        frames,
        started,
        missing);
    if (started && missing > 0)
    {
        m_playback_started.store(false, std::memory_order_release);
        m_ring.notePrefillStart();
    }
    m_callback_active.store(false, std::memory_order_release);
}

void AudioManager::logSummary(bool force)
{
    const auto now = steadySeconds();
    auto last = m_last_summary_second.load(std::memory_order_relaxed);
    if (!force)
    {
        if (now <= last ||
            !m_last_summary_second.compare_exchange_strong(
                last,
                now,
                std::memory_order_relaxed))
        {
            return;
        }
    }

    const auto stats = m_ring.stats();
    const auto callback_interval_count =
        m_callback_interval_count.load(std::memory_order_relaxed);
    const auto callback_interval_total_us =
        m_callback_interval_total_us.load(std::memory_order_relaxed);
    const auto callback_interval_min_us =
        m_callback_interval_min_us.load(std::memory_order_relaxed);
    const auto pcm_sample_count =
        m_pcm_sample_count.load(std::memory_order_relaxed);
    const auto pcm_sample_sum =
        m_pcm_sample_sum.load(std::memory_order_relaxed);
    const auto pcm_square_sum =
        m_pcm_square_sum.load(std::memory_order_relaxed);
    const auto pcm_min_block_frames =
        m_pcm_min_block_frames.load(std::memory_order_relaxed);
    const auto producer_lock_count =
        m_producer_lock_count.load(std::memory_order_relaxed);
    const double pcm_rms =
        pcm_sample_count == 0
            ? 0.0
            : std::sqrt(
                  static_cast<double>(pcm_square_sum) /
                  static_cast<double>(pcm_sample_count));
    const double pcm_dc =
        pcm_sample_count == 0
            ? 0.0
            : static_cast<double>(pcm_sample_sum) /
                  static_cast<double>(pcm_sample_count);
    brls::Logger::info(
        "audio-summary requested_freq={} obtained_freq={} "
        "requested_channels={} obtained_channels={} requested_format={} "
        "obtained_format={} requested_samples={} obtained_samples={} "
        "decoded_pcm_blocks={} ring_current_samples={} ring_max_samples={} "
        "prefill_starts={} prefill_completions={} underrun_callbacks={} "
        "underrun_missing_samples={} dropped_oldest_blocks={} "
        "dropped_oldest_samples={} callback_count={} callback_frames={} "
        "callback_started_frames={} callback_bad_lengths={} "
        "callback_interval_count={} callback_interval_avg_us={} "
        "callback_interval_min_us={} callback_interval_max_us={} "
        "producer_lock_count={} producer_lock_wait_avg_us={} "
        "producer_lock_wait_max_us={} pcm_samples={} pcm_rms={:.2f} "
        "pcm_dc={:.2f} pcm_abs_peak={} pcm_clipped_samples={} "
        "pcm_zero_samples={} pcm_min_block_frames={} "
        "pcm_max_block_frames={} open_errors={}",
        m_requested.freq,
        m_obtained.freq,
        m_requested.channels,
        m_obtained.channels,
        static_cast<int>(m_requested.format),
        static_cast<int>(m_obtained.format),
        m_requested.samples,
        m_obtained.samples,
        stats.decoded_blocks,
        stats.current_frames,
        stats.max_frames,
        stats.prefill_starts,
        stats.prefill_completions,
        stats.underrun_callbacks,
        stats.underrun_missing_frames,
        stats.dropped_oldest_blocks,
        stats.dropped_oldest_frames,
        stats.callback_count,
        m_callback_frames.load(std::memory_order_relaxed),
        m_callback_started_frames.load(std::memory_order_relaxed),
        m_callback_bad_lengths.load(std::memory_order_relaxed),
        callback_interval_count,
        callback_interval_count == 0
            ? 0
            : callback_interval_total_us / callback_interval_count,
        callback_interval_min_us ==
                std::numeric_limits<std::uint64_t>::max()
            ? 0
            : callback_interval_min_us,
        m_callback_interval_max_us.load(std::memory_order_relaxed),
        producer_lock_count,
        producer_lock_count == 0
            ? 0
            : m_producer_lock_wait_total_us.load(
                  std::memory_order_relaxed) /
                  producer_lock_count,
        m_producer_lock_wait_max_us.load(std::memory_order_relaxed),
        pcm_sample_count,
        pcm_rms,
        pcm_dc,
        m_pcm_abs_peak.load(std::memory_order_relaxed),
        m_pcm_clipped_samples.load(std::memory_order_relaxed),
        m_pcm_zero_samples.load(std::memory_order_relaxed),
        pcm_min_block_frames ==
                std::numeric_limits<std::uint64_t>::max()
            ? 0
            : pcm_min_block_frames,
        m_pcm_max_block_frames.load(std::memory_order_relaxed),
        m_open_errors.load(std::memory_order_relaxed));
}
