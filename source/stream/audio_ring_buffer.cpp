#include "stream/audio_ring_buffer.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace {

std::size_t framesForMilliseconds(
    unsigned int rate,
    unsigned int milliseconds)
{
    return static_cast<std::size_t>(rate) * milliseconds / 1000;
}

} // namespace

std::size_t AudioRingBuffer::framesToSamples(
    std::size_t frames,
    unsigned int channels)
{
    return frames * channels;
}

std::size_t AudioRingBuffer::controlBlockFrames() const
{
    return control_block_frames_;
}

bool AudioRingBuffer::configured() const
{
    return configured_.load(std::memory_order_acquire);
}

bool AudioRingBuffer::configure(
    unsigned int rate,
    unsigned int channels,
    unsigned int prefill_ms,
    unsigned int low_watermark_ms,
    unsigned int target_watermark_ms,
    unsigned int high_watermark_ms,
    unsigned int capacity_ms,
    unsigned int control_block_ms)
{
    configured_.store(false, std::memory_order_release);
    if (rate == 0 || channels == 0 || high_watermark_ms == 0 ||
        capacity_ms <= high_watermark_ms ||
        low_watermark_ms >= target_watermark_ms ||
        target_watermark_ms >= high_watermark_ms ||
        control_block_ms == 0)
    {
        return false;
    }

    const std::size_t capacity_frames =
        framesForMilliseconds(rate, capacity_ms);
    if (capacity_frames == 0 ||
        channels >
            std::numeric_limits<std::size_t>::max() / capacity_frames)
    {
        return false;
    }

    try
    {
        samples_.assign(
            framesToSamples(capacity_frames, channels),
            0);
    }
    catch (...)
    {
        samples_.clear();
        return false;
    }

    rate_ = rate;
    channels_ = channels;
    capacity_frames_ = capacity_frames;
    prefill_frames_ = framesForMilliseconds(rate, prefill_ms);
    low_watermark_frames_ = framesForMilliseconds(rate, low_watermark_ms);
    target_watermark_frames_ =
        framesForMilliseconds(rate, target_watermark_ms);
    high_watermark_frames_ =
        framesForMilliseconds(rate, high_watermark_ms);
    control_block_frames_ =
        framesForMilliseconds(rate, control_block_ms);

    if (prefill_frames_ == 0 || control_block_frames_ == 0 ||
        low_watermark_frames_ > target_watermark_frames_ ||
        target_watermark_frames_ >= high_watermark_frames_ ||
        high_watermark_frames_ > capacity_frames_)
    {
        samples_.clear();
        return false;
    }

    read_sequence_.store(0, std::memory_order_relaxed);
    discard_sequence_.store(0, std::memory_order_relaxed);
    write_sequence_.store(0, std::memory_order_relaxed);
    max_queued_frames_.store(0, std::memory_order_relaxed);
    decoded_blocks_.store(0, std::memory_order_relaxed);
    prefill_starts_.store(0, std::memory_order_relaxed);
    prefill_completions_.store(0, std::memory_order_relaxed);
    underrun_callbacks_.store(0, std::memory_order_relaxed);
    underrun_missing_frames_.store(0, std::memory_order_relaxed);
    dropped_oldest_blocks_.store(0, std::memory_order_relaxed);
    dropped_oldest_frames_.store(0, std::memory_order_relaxed);
    callback_count_.store(0, std::memory_order_relaxed);
    clear_count_.store(0, std::memory_order_relaxed);
    configured_.store(true, std::memory_order_release);
    return true;
}

void AudioRingBuffer::clear()
{
    if (!configured())
    {
        return;
    }

    read_sequence_.store(0, std::memory_order_relaxed);
    discard_sequence_.store(0, std::memory_order_relaxed);
    write_sequence_.store(0, std::memory_order_relaxed);
    max_queued_frames_.store(0, std::memory_order_relaxed);
    clear_count_.fetch_add(1, std::memory_order_relaxed);
}

bool AudioRingBuffer::producerWrite(
    const int16_t* samples,
    std::size_t frames)
{
    if (!configured() || !samples || frames == 0)
    {
        return false;
    }

    decoded_blocks_.fetch_add(1, std::memory_order_relaxed);
    if (frames > capacity_frames_)
    {
        samples += framesToSamples(
            frames - capacity_frames_,
            channels_);
        frames = capacity_frames_;
    }

    const auto write_sequence =
        write_sequence_.load(std::memory_order_relaxed);
    const auto effective_read_sequence = std::max(
        read_sequence_.load(std::memory_order_acquire),
        discard_sequence_.load(std::memory_order_acquire));
    const std::size_t queued =
        write_sequence > effective_read_sequence
            ? static_cast<std::size_t>(
                  write_sequence - effective_read_sequence)
            : 0;
    std::size_t queued_after_discard = queued;
    if (queued + frames > high_watermark_frames_)
    {
        const std::size_t projected = queued + frames;
        const std::size_t frames_over =
            projected > target_watermark_frames_
                ? projected - target_watermark_frames_
                : 0;
        std::size_t discard_frames =
            frames_over == 0
                ? 0
                : ((frames_over + controlBlockFrames() - 1) /
                   controlBlockFrames()) *
                      controlBlockFrames();
        discard_frames = std::min(discard_frames, queued);

        if (discard_frames > 0)
        {
            const std::size_t discarded_blocks =
                (discard_frames + controlBlockFrames() - 1) /
                controlBlockFrames();
            const auto discard_target =
                effective_read_sequence + discard_frames;
            auto observed_discard =
                discard_sequence_.load(std::memory_order_relaxed);
            while (observed_discard < discard_target &&
                   !discard_sequence_.compare_exchange_weak(
                       observed_discard,
                       discard_target,
                       std::memory_order_release,
                       std::memory_order_relaxed))
            {
            }
            queued_after_discard -= discard_frames;
            dropped_oldest_blocks_.fetch_add(
                discarded_blocks,
                std::memory_order_relaxed);
            dropped_oldest_frames_.fetch_add(
                discard_frames,
                std::memory_order_relaxed);
        }
    }

    const std::size_t writable =
        capacity_frames_ - queued_after_discard;
    if (frames > writable)
    {
        frames = writable;
    }
    if (frames == 0)
    {
        return true;
    }

    const std::size_t write = static_cast<std::size_t>(
        write_sequence % capacity_frames_);
    const std::size_t first =
        std::min(frames, capacity_frames_ - write);
    std::memcpy(
        samples_.data() + framesToSamples(write, channels_),
        samples,
        framesToSamples(first, channels_) * sizeof(int16_t));
    if (frames > first)
    {
        std::memcpy(
            samples_.data(),
            samples + framesToSamples(first, channels_),
            framesToSamples(frames - first, channels_) *
                sizeof(int16_t));
    }

    write_sequence_.store(
        write_sequence + frames,
        std::memory_order_release);
    const auto published_read = std::max(
        read_sequence_.load(std::memory_order_acquire),
        discard_sequence_.load(std::memory_order_acquire));
    const std::size_t queued_now = static_cast<std::size_t>(
        (write_sequence + frames) - published_read);
    std::size_t observed =
        max_queued_frames_.load(std::memory_order_relaxed);
    while (queued_now > observed &&
           !max_queued_frames_.compare_exchange_weak(
               observed,
               queued_now,
               std::memory_order_relaxed))
    {
    }
    return true;
}

void AudioRingBuffer::consumerRead(
    int16_t* output,
    std::size_t frames,
    bool callback_started,
    std::size_t missing_frames)
{
    if (!configured() || !output || frames == 0)
    {
        return;
    }

    callback_count_.fetch_add(1, std::memory_order_relaxed);
    if (!callback_started)
    {
        std::memset(
            output,
            0,
            framesToSamples(frames, channels_) * sizeof(int16_t));
        return;
    }

    const auto read_sequence = std::max(
        read_sequence_.load(std::memory_order_relaxed),
        discard_sequence_.load(std::memory_order_acquire));
    const auto write_sequence =
        write_sequence_.load(std::memory_order_acquire);
    const std::size_t queued =
        write_sequence > read_sequence
            ? static_cast<std::size_t>(write_sequence - read_sequence)
            : 0;
    const std::size_t readable = std::min(frames, queued);

    if (readable > 0)
    {
        const std::size_t read = static_cast<std::size_t>(
            read_sequence % capacity_frames_);
        const std::size_t first =
            std::min(readable, capacity_frames_ - read);
        std::memcpy(
            output,
            samples_.data() + framesToSamples(read, channels_),
            framesToSamples(first, channels_) * sizeof(int16_t));
        if (readable > first)
        {
            std::memcpy(
                output + framesToSamples(first, channels_),
                samples_.data(),
                framesToSamples(readable - first, channels_) *
                    sizeof(int16_t));
        }
        read_sequence_.store(
            read_sequence + readable,
            std::memory_order_release);
    }

    if (readable < frames)
    {
        const std::size_t padding = frames - readable;
        std::memset(
            output + framesToSamples(readable, channels_),
            0,
            framesToSamples(padding, channels_) * sizeof(int16_t));

        if (callback_started && padding > 0)
        {
            underrun_callbacks_.fetch_add(
                1,
                std::memory_order_relaxed);
            underrun_missing_frames_.fetch_add(
                std::max(padding, missing_frames),
                std::memory_order_relaxed);
        }
    }

    const auto remaining_write =
        write_sequence_.load(std::memory_order_acquire);
    const auto remaining_read = std::max(
        read_sequence_.load(std::memory_order_relaxed),
        discard_sequence_.load(std::memory_order_acquire));
    const std::size_t remaining =
        remaining_write > remaining_read
            ? static_cast<std::size_t>(
                  remaining_write - remaining_read)
            : 0;
    std::size_t observed =
        max_queued_frames_.load(std::memory_order_relaxed);
    while (remaining > observed &&
           !max_queued_frames_.compare_exchange_weak(
               observed,
               remaining,
               std::memory_order_relaxed))
    {
    }
}

void AudioRingBuffer::notePrefillStart()
{
    prefill_starts_.fetch_add(1, std::memory_order_relaxed);
}

void AudioRingBuffer::notePrefillComplete()
{
    prefill_completions_.fetch_add(1, std::memory_order_relaxed);
}

std::size_t AudioRingBuffer::currentFrames() const
{
    const auto read_sequence = std::max(
        read_sequence_.load(std::memory_order_acquire),
        discard_sequence_.load(std::memory_order_acquire));
    const auto write_sequence =
        write_sequence_.load(std::memory_order_acquire);
    return write_sequence > read_sequence
               ? static_cast<std::size_t>(
                     write_sequence - read_sequence)
               : 0;
}

std::size_t AudioRingBuffer::capacityFrames() const
{
    return capacity_frames_;
}

AudioRingBufferStats AudioRingBuffer::stats() const
{
    AudioRingBufferStats result;
    result.current_frames = currentFrames();
    result.max_frames =
        max_queued_frames_.load(std::memory_order_relaxed);
    result.decoded_blocks =
        decoded_blocks_.load(std::memory_order_relaxed);
    result.prefill_starts =
        prefill_starts_.load(std::memory_order_relaxed);
    result.prefill_completions =
        prefill_completions_.load(std::memory_order_relaxed);
    result.underrun_callbacks =
        underrun_callbacks_.load(std::memory_order_relaxed);
    result.underrun_missing_frames =
        underrun_missing_frames_.load(std::memory_order_relaxed);
    result.dropped_oldest_blocks =
        dropped_oldest_blocks_.load(std::memory_order_relaxed);
    result.dropped_oldest_frames =
        dropped_oldest_frames_.load(std::memory_order_relaxed);
    result.callback_count =
        callback_count_.load(std::memory_order_relaxed);
    result.clear_count =
        clear_count_.load(std::memory_order_relaxed);
    return result;
}
