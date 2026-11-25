#ifndef AKIRA_IO_AUDIO_MANAGER_HPP
#define AKIRA_IO_AUDIO_MANAGER_HPP

#include <SDL2/SDL.h>
#include <cstdint>
#include <span>
#include <vector>

#include "play/audio_gain.hpp"
#include "play/audio_output.hpp"

namespace akira::play {

class AudioManager final : public AudioOutput
{
public:
    AudioManager() = default;
    ~AudioManager() override;

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    auto open(unsigned channels, unsigned rate) -> std::expected<void, std::string> override;
    void play(std::span<const std::int16_t> samples) override;
    void close() override;
    void setVolume(Volume volume) override;

private:
    static constexpr int queueLimit = 16000;

    AudioGain m_gain;
    std::vector<std::int16_t> m_boosted;
    SDL_AudioDeviceID m_device_id = 0;
};

}

#endif // AKIRA_IO_AUDIO_MANAGER_HPP
