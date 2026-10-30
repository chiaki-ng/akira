#ifndef AKIRA_IO_AUDIO_MANAGER_HPP
#define AKIRA_IO_AUDIO_MANAGER_HPP

#include "stream/audio_ring_buffer.hpp"

#include <SDL2/SDL.h>
#include <atomic>
#include <cstdint>
#include <chiaki/log.h>
#include <limits>

class AudioManager
{
public:
    AudioManager();
    ~AudioManager();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    void setLogger(ChiakiLog* log) { m_log = log; }

    bool init(unsigned int channels, unsigned int rate);
    void play(int16_t* buf, size_t samples_count);
    void cleanup();

    bool isInitialized() const { return m_device_id > 0; }

private:
    static void SDLCALL sdlAudioCallback(
        void* userdata,
        Uint8* stream,
        int len);

    void audioCallback(Uint8* stream, int len);
    void logSummary(bool force);
    void resetDiagnostics();
    void resetRing();
    std::size_t prefillFrames() const;

    ChiakiLog* m_log = nullptr;
    SDL_AudioDeviceID m_device_id = 0;
    SDL_AudioSpec m_requested{};
    SDL_AudioSpec m_obtained{};
    AudioRingBuffer m_ring;
    std::atomic<bool> m_playback_started{false};
    std::atomic<bool> m_callback_active{false};
    std::atomic<bool> m_shutdown{false};
    std::atomic<std::uint64_t> m_open_errors{0};
    std::atomic<std::uint64_t> m_last_summary_second{0};
    std::atomic<std::uint64_t> m_last_callback_us{0};
    std::atomic<std::uint64_t> m_callback_frames{0};
    std::atomic<std::uint64_t> m_callback_started_frames{0};
    std::atomic<std::uint64_t> m_callback_interval_count{0};
    std::atomic<std::uint64_t> m_callback_interval_total_us{0};
    std::atomic<std::uint64_t> m_callback_interval_min_us{
        std::numeric_limits<std::uint64_t>::max()};
    std::atomic<std::uint64_t> m_callback_interval_max_us{0};
    std::atomic<std::uint64_t> m_callback_bad_lengths{0};
    std::atomic<std::uint64_t> m_producer_lock_count{0};
    std::atomic<std::uint64_t> m_producer_lock_wait_total_us{0};
    std::atomic<std::uint64_t> m_producer_lock_wait_max_us{0};
    std::atomic<std::uint64_t> m_pcm_sample_count{0};
    std::atomic<std::int64_t> m_pcm_sample_sum{0};
    std::atomic<std::uint64_t> m_pcm_square_sum{0};
    std::atomic<std::uint64_t> m_pcm_clipped_samples{0};
    std::atomic<std::uint64_t> m_pcm_zero_samples{0};
    std::atomic<std::uint64_t> m_pcm_abs_peak{0};
    std::atomic<std::uint64_t> m_pcm_min_block_frames{
        std::numeric_limits<std::uint64_t>::max()};
    std::atomic<std::uint64_t> m_pcm_max_block_frames{0};
};

#endif // AKIRA_IO_AUDIO_MANAGER_HPP
