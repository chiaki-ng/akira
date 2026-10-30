#ifndef AKIRA_STREAM_AUDIO_RING_BUFFER_HPP
#define AKIRA_STREAM_AUDIO_RING_BUFFER_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

struct AudioRingBufferStats
{
    std::size_t current_frames = 0;
    std::size_t max_frames = 0;
    std::uint64_t decoded_blocks = 0;
    std::uint64_t prefill_starts = 0;
    std::uint64_t prefill_completions = 0;
    std::uint64_t underrun_callbacks = 0;
    std::uint64_t underrun_missing_frames = 0;
    std::uint64_t dropped_oldest_blocks = 0;
    std::uint64_t dropped_oldest_frames = 0;
    std::uint64_t callback_count = 0;
    std::uint64_t clear_count = 0;
};

class AudioRingBuffer
{
public:
    AudioRingBuffer() = default;
    AudioRingBuffer(const AudioRingBuffer&) = delete;
    AudioRingBuffer& operator=(const AudioRingBuffer&) = delete;

    bool configure(
        unsigned int rate,
        unsigned int channels,
        unsigned int prefill_ms,
        unsigned int low_watermark_ms,
        unsigned int target_watermark_ms,
        unsigned int high_watermark_ms,
        unsigned int capacity_ms,
        unsigned int control_block_ms);

    void clear();

    bool producerWrite(const int16_t* samples, std::size_t frames);

    void consumerRead(
        int16_t* output,
        std::size_t frames,
        bool callback_started,
        std::size_t missing_frames);

    void notePrefillStart();
    void notePrefillComplete();

    std::size_t currentFrames() const;
    std::size_t capacityFrames() const;
    unsigned int channels() const { return channels_; }
    unsigned int rate() const { return rate_; }
    bool configured() const;

    AudioRingBufferStats stats() const;

private:
    static std::size_t framesToSamples(
        std::size_t frames,
        unsigned int channels);

    std::size_t controlBlockFrames() const;

    std::vector<int16_t> samples_;
    std::atomic<std::uint64_t> read_sequence_{0};
    std::atomic<std::uint64_t> discard_sequence_{0};
    std::atomic<std::uint64_t> write_sequence_{0};
    std::atomic<std::size_t> max_queued_frames_{0};
    std::atomic<bool> configured_{false};

    unsigned int rate_ = 0;
    unsigned int channels_ = 0;
    std::size_t capacity_frames_ = 0;
    std::size_t prefill_frames_ = 0;
    std::size_t low_watermark_frames_ = 0;
    std::size_t target_watermark_frames_ = 0;
    std::size_t high_watermark_frames_ = 0;
    std::size_t control_block_frames_ = 0;

    std::atomic<std::uint64_t> decoded_blocks_{0};
    std::atomic<std::uint64_t> prefill_starts_{0};
    std::atomic<std::uint64_t> prefill_completions_{0};
    std::atomic<std::uint64_t> underrun_callbacks_{0};
    std::atomic<std::uint64_t> underrun_missing_frames_{0};
    std::atomic<std::uint64_t> dropped_oldest_blocks_{0};
    std::atomic<std::uint64_t> dropped_oldest_frames_{0};
    std::atomic<std::uint64_t> callback_count_{0};
    std::atomic<std::uint64_t> clear_count_{0};
};

#endif // AKIRA_STREAM_AUDIO_RING_BUFFER_HPP
