#pragma once

#include <cstdint>
#include <mutex>
#include <optional>

#include <arisa/provider/audio/app_voice.hpp>
#include <arisa/provider/audio/provider.hpp>

#include "play/audio_output.hpp"

namespace akira::app::arisa {

class StreamVoice final : public play::AudioOutput {
public:
    static constexpr std::uint32_t bufferFrames = 480;
    static constexpr std::uint32_t bufferCount = 8;

    explicit StreamVoice(::arisa::provider::AudioProvider& audio);
    ~StreamVoice() override;

    StreamVoice(const StreamVoice&) = delete;
    StreamVoice& operator=(const StreamVoice&) = delete;

    auto open(unsigned channels, unsigned rate) -> std::expected<void, std::string> override;
    void play(std::span<const std::int16_t> samples) override;
    void close() override;

private:
    ::arisa::provider::AudioProvider& audio;
    std::mutex mutex;
    std::optional<::arisa::provider::AppVoice> voice;
    std::uint64_t droppedFrames = 0;
};

}
