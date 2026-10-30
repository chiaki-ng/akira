#include "test_util.hpp"

#include "stream/audio_ring_buffer.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace {

std::unique_ptr<AudioRingBuffer> make_ring(
    unsigned int rate = 48'000,
    unsigned int channels = 2)
{
    auto ring = std::make_unique<AudioRingBuffer>();
    CHECK(ring->configure(
        rate,
        channels,
        30,
        20,
        30,
        50,
        60,
        10));
    return ring;
}

std::vector<int16_t> make_signal(
    std::size_t frames,
    unsigned int channels,
    int16_t start)
{
    std::vector<int16_t> result(frames * channels);
    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        for (unsigned int channel = 0; channel < channels; ++channel)
        {
            result[frame * channels + channel] =
                static_cast<int16_t>(
                    start + static_cast<int16_t>(frame + channel));
        }
    }
    return result;
}

void check_signal(
    const std::vector<int16_t>& actual,
    const std::vector<int16_t>& expected)
{
    CHECK_EQ(actual.size(), expected.size());
    const auto count = std::min(actual.size(), expected.size());
    const bool equal = std::equal(
        actual.begin(),
        actual.begin() + count,
        expected.begin());
    if (!equal)
    {
        std::size_t first_difference = 0;
        while (first_difference < count &&
               actual[first_difference] == expected[first_difference])
        {
            ++first_difference;
        }
        CHECK_EQ(actual[first_difference], expected[first_difference]);
    }
}

} // namespace

TEST(audio_ring_prefill_waits_and_outputs_in_order)
{
    auto ring = make_ring();
    const auto signal = make_signal(480, 2, 100);
    std::vector<int16_t> output(480 * 2);

    CHECK(ring->producerWrite(signal.data(), 480));
    CHECK_EQ(ring->currentFrames(), 480u);
    ring->consumerRead(output.data(), 480, false, 0);
    CHECK(std::all_of(
        output.begin(),
        output.end(),
        [](int16_t value) { return value == 0; }));
    CHECK_EQ(ring->currentFrames(), 480u);

    ring->consumerRead(output.data(), 480, true, 0);
    check_signal(output, signal);
    CHECK_EQ(ring->currentFrames(), 0u);
    CHECK_EQ(ring->stats().underrun_callbacks, 0u);
}

TEST(audio_ring_wraps_without_losing_order)
{
    auto ring = make_ring();
    const auto first = make_signal(960, 2, 1000);
    const auto second = make_signal(960, 2, 3000);
    std::vector<int16_t> output(960 * 2);

    CHECK(ring->producerWrite(first.data(), 960));
    ring->consumerRead(output.data(), 960, true, 0);
    check_signal(output, first);

    CHECK(ring->producerWrite(second.data(), 960));
    ring->consumerRead(output.data(), 960, true, 0);
    check_signal(output, second);
    CHECK_EQ(ring->currentFrames(), 0u);
}

TEST(audio_ring_accepts_480_frame_writes_with_1024_frame_callbacks)
{
    auto ring = make_ring();
    std::vector<int16_t> queued;
    for (int block = 0; block < 5; ++block)
    {
        const auto signal = make_signal(
            480,
            2,
            static_cast<int16_t>(block * 1000));
        queued.insert(queued.end(), signal.begin(), signal.end());
        CHECK(ring->producerWrite(signal.data(), 480));
    }

    std::vector<int16_t> first_output(1024 * 2);
    ring->consumerRead(first_output.data(), 1024, true, 0);
    check_signal(
        first_output,
        std::vector<int16_t>(
            queued.begin(),
            queued.begin() + 1024 * 2));

    std::vector<int16_t> second_output(1024 * 2);
    ring->consumerRead(second_output.data(), 1024, true, 0);
    check_signal(
        second_output,
        std::vector<int16_t>(
            queued.begin() + 1024 * 2,
            queued.begin() + 2048 * 2));
    CHECK_EQ(ring->currentFrames(), 352u);
}

TEST(audio_ring_high_watermark_drops_oldest_blocks)
{
    auto ring = make_ring();
    const auto first = make_signal(2400, 2, 100);
    const auto second = make_signal(480, 2, 9000);
    std::vector<int16_t> output(960 * 2);

    CHECK(ring->producerWrite(first.data(), 2400));
    CHECK_EQ(ring->currentFrames(), 2400u);
    CHECK(ring->producerWrite(second.data(), 480));

    const auto stats = ring->stats();
    CHECK_EQ(stats.dropped_oldest_blocks, 3u);
    CHECK_EQ(stats.dropped_oldest_frames, 1440u);
    CHECK_EQ(ring->currentFrames(), 1440u);

    ring->consumerRead(output.data(), 960, true, 0);
    check_signal(
        output,
        std::vector<int16_t>(
            first.begin() + 1440 * 2,
            first.begin() + 2400 * 2));
}

TEST(audio_ring_high_watermark_uses_absolute_sequence_after_playback)
{
    auto ring = make_ring();
    const auto warmup = make_signal(480, 2, 10);
    const auto queued = make_signal(2400, 2, 100);
    const auto incoming = make_signal(480, 2, 9000);
    std::vector<int16_t> output(480 * 2);

    for (int i = 0; i < 10; ++i)
    {
        CHECK(ring->producerWrite(warmup.data(), 480));
        ring->consumerRead(output.data(), 480, true, 0);
    }

    CHECK(ring->producerWrite(queued.data(), 2400));
    CHECK_EQ(ring->currentFrames(), 2400u);
    CHECK(ring->producerWrite(incoming.data(), 480));
    CHECK_EQ(ring->currentFrames(), 1440u);
    CHECK(ring->currentFrames() <= ring->capacityFrames());

    const auto stats = ring->stats();
    CHECK_EQ(stats.dropped_oldest_blocks, 3u);
    CHECK_EQ(stats.dropped_oldest_frames, 1440u);

    std::vector<int16_t> recovered(960 * 2);
    ring->consumerRead(recovered.data(), 960, true, 0);
    check_signal(
        recovered,
        std::vector<int16_t>(
            queued.begin() + 1440 * 2,
            queued.end()));
}

TEST(audio_ring_repeated_high_watermark_drops_stay_within_capacity)
{
    auto ring = make_ring();
    const auto block = make_signal(480, 2, 100);
    std::vector<int16_t> output(480 * 2);

    for (int i = 0; i < 100; ++i)
    {
        CHECK(ring->producerWrite(block.data(), 480));
        ring->consumerRead(output.data(), 480, true, 0);
    }

    const auto backlog = make_signal(2400, 2, 1000);
    CHECK(ring->producerWrite(backlog.data(), 2400));
    for (int i = 0; i < 100; ++i)
    {
        CHECK(ring->producerWrite(block.data(), 480));
        CHECK(ring->currentFrames() <= ring->capacityFrames());
    }
}

TEST(audio_ring_underrun_pads_silence_and_counts)
{
    auto ring = make_ring();
    const auto signal = make_signal(240, 2, 100);
    std::vector<int16_t> output(480 * 2);

    CHECK(ring->producerWrite(signal.data(), 240));
    ring->consumerRead(output.data(), 480, true, 240);
    check_signal(
        std::vector<int16_t>(output.begin(), output.begin() + 480),
        signal);
    CHECK(std::all_of(
        output.begin() + 480,
        output.end(),
        [](int16_t value) { return value == 0; }));

    const auto stats = ring->stats();
    CHECK_EQ(stats.underrun_callbacks, 1u);
    CHECK_EQ(stats.underrun_missing_frames, 240u);
}

TEST(audio_ring_clear_discards_previous_session)
{
    auto ring = make_ring();
    const auto signal = make_signal(960, 2, 100);
    std::vector<int16_t> output(960 * 2);

    CHECK(ring->producerWrite(signal.data(), 960));
    ring->clear();
    CHECK_EQ(ring->currentFrames(), 0u);
    ring->consumerRead(output.data(), 960, true, 960);
    CHECK(std::all_of(
        output.begin(),
        output.end(),
        [](int16_t value) { return value == 0; }));
    CHECK_EQ(ring->stats().clear_count, 1u);
}

TEST(audio_ring_supports_non_stereo_parameters)
{
    auto ring = make_ring(44'100, 1);
    const auto signal = make_signal(441, 1, 500);
    std::vector<int16_t> output(441);

    CHECK(ring->producerWrite(signal.data(), 441));
    ring->consumerRead(output.data(), 441, true, 0);
    check_signal(output, signal);
    CHECK_EQ(ring->currentFrames(), 0u);
}

TEST(audio_ring_concurrent_producer_and_consumer_stays_bounded)
{
    auto ring = make_ring();
    constexpr std::size_t block_frames = 48;
    constexpr int iterations = 5000;
    const auto signal = make_signal(block_frames, 2, 1);
    std::vector<int16_t> output(block_frames * 2);
    std::atomic<bool> failed{false};

    std::thread producer([&] {
        for (int i = 0; i < iterations; ++i)
        {
            if (!ring->producerWrite(signal.data(), block_frames))
            {
                failed.store(true);
                return;
            }
        }
    });
    std::thread consumer([&] {
        for (int i = 0; i < iterations; ++i)
        {
            ring->consumerRead(output.data(), block_frames, true, 0);
        }
    });
    producer.join();
    consumer.join();

    CHECK(!failed.load());
    CHECK(ring->currentFrames() <= ring->capacityFrames());
}

TEST(audio_ring_concurrent_high_watermark_drops_stay_bounded)
{
    auto ring = make_ring();
    constexpr std::size_t block_frames = 960;
    constexpr int iterations = 2000;
    const auto signal = make_signal(block_frames, 2, 1);
    std::vector<int16_t> output(block_frames * 2);
    std::atomic<bool> start{false};

    std::thread producer([&] {
        while (!start.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
        for (int i = 0; i < iterations; ++i)
        {
            CHECK(ring->producerWrite(signal.data(), block_frames));
        }
    });
    std::thread consumer([&] {
        while (!start.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
        for (int i = 0; i < iterations; ++i)
        {
            ring->consumerRead(output.data(), block_frames / 4, true, 0);
            std::this_thread::yield();
        }
    });
    start.store(true, std::memory_order_release);
    producer.join();
    consumer.join();

    CHECK(ring->currentFrames() <= ring->capacityFrames());
    CHECK(ring->stats().dropped_oldest_frames > 0);
}
