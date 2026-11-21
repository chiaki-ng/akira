#include "app/arisa/stream_voice.hpp"

#include <format>
#include <utility>

#include "log/log.hpp"
#include "play/audio_gain.hpp"

namespace akira::app::arisa {

namespace provider = ::arisa::provider;

StreamVoice::StreamVoice(provider::AudioProvider& audio)
    : audio(audio)
{
}

StreamVoice::~StreamVoice()
{
    close();
}

auto StreamVoice::open(unsigned channels, unsigned rate) -> std::expected<void, std::string>
{
    if (channels != provider::AppVoice::channels || rate != provider::AppVoice::sample_rate)
        return std::unexpected(std::format("the app voice plays {} channels at {} Hz, the stream sends {} at {} Hz",
            provider::AppVoice::channels, provider::AppVoice::sample_rate, channels, rate));
    std::expected<provider::AppVoice, provider::ProviderError> opened = audio.open_voice(provider::AppVoiceOptions{
        .buffer_frames = provider::AudioFrames::of<bufferFrames>(),
        .buffer_count = provider::AudioBufferCount::of<bufferCount>(),
        .ui_sounds = provider::UiSounds::duck,
    });
    if (!opened)
        return std::unexpected(std::format("open_voice refused: {}", opened.error().explanation()));
    const std::scoped_lock lock(mutex);
    voice.emplace(std::move(*opened));
    droppedFrames = 0;
    log::info("[AUDIO] app voice open: {} buffers of {} frames", bufferCount, bufferFrames);
    return {};
}

void StreamVoice::play(std::span<const std::int16_t> samples)
{
    const std::scoped_lock lock(mutex);
    if (!voice)
        return;
    while (!samples.empty())
    {
        std::optional<provider::AppVoice::Buffer> buffer = voice->acquire();
        if (!buffer)
        {
            droppedFrames += samples.size() / provider::AppVoice::channels;
            return;
        }
        const std::size_t written = play::AudioGain::apply(samples, buffer->samples());
        const auto frames = static_cast<std::uint32_t>(written / provider::AppVoice::channels);
        if (const auto submitted = buffer->submit(frames); !submitted)
        {
            log::error("[AUDIO] app voice submit refused: {}", submitted.error().explanation());
            return;
        }
        samples = samples.subspan(written);
    }
}

void StreamVoice::close()
{
    const std::scoped_lock lock(mutex);
    if (!voice)
        return;
    const provider::AppVoiceStatistics counted = voice->statistics();
    log::info("[AUDIO] app voice closed: {} submitted, {} played, {} underruns, {} frames dropped on a full queue",
        counted.submitted, counted.played, counted.underruns, droppedFrames);
    voice.reset();
}

}
