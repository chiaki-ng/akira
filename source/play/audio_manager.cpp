#include "play/audio_manager.hpp"

#include "log/log.hpp"

namespace akira::play {

AudioManager::~AudioManager()
{
    close();
}

auto AudioManager::open(unsigned channels, unsigned rate) -> std::expected<void, std::string>
{
    SDL_AudioSpec want;
    SDL_memset(&want, 0, sizeof(want));
    want.freq = static_cast<int>(rate);
    want.format = AUDIO_S16SYS;
    want.channels = static_cast<Uint8>(channels);
    want.samples = 1024;
    want.callback = nullptr;

    if (m_device_id == 0)
        m_device_id = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (m_device_id == 0)
        return std::unexpected(std::string("SDL_OpenAudioDevice failed: ") + SDL_GetError());
    SDL_PauseAudioDevice(m_device_id, 0);
    return {};
}

void AudioManager::play(std::span<const std::int16_t> samples)
{
    m_boosted.resize(samples.size());
    m_gain.apply(samples, m_boosted);

    const Uint32 queued = SDL_GetQueuedAudioSize(m_device_id);
    if (queued > queueLimit)
    {
        log::warning("Triggering SDL_ClearQueuedAudio with queue size = {}", queued);
        SDL_ClearQueuedAudio(m_device_id);
    }

    if (SDL_QueueAudio(m_device_id, m_boosted.data(), static_cast<Uint32>(sizeof(std::int16_t) * m_boosted.size())) != 0)
        log::error("SDL_QueueAudio failed: {}", SDL_GetError());
}

void AudioManager::setVolume(Volume volume)
{
    m_gain.setVolume(volume);
}

void AudioManager::close()
{
    if (m_device_id == 0)
        return;
    SDL_CloseAudioDevice(m_device_id);
    m_device_id = 0;
}

}
