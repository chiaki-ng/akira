#include "stream/audio_manager.hpp"
#include <borealis.hpp>
#include <cstring>

AudioManager::AudioManager()
{
}

AudioManager::~AudioManager()
{
    cleanup();
}

void AudioManager::init(unsigned int channels, unsigned int rate)
{
    SDL_AudioSpec want;
    SDL_memset(&want, 0, sizeof(want));

    want.freq = rate;
    want.format = AUDIO_S16SYS;
    want.channels = channels;
    m_channels = channels;
    want.samples = 1024;
    want.callback = NULL;

    if (m_device_id <= 0)
    {
        m_device_id = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    }

    if (m_device_id <= 0)
    {
        brls::Logger::error("SDL_OpenAudioDevice failed: {}", SDL_GetError());
    }
    else
    {
        SDL_PauseAudioDevice(m_device_id, 0);
    }
}

void AudioManager::play(std::span<const int16_t> samples)
{
    m_boosted.assign(samples.begin(), samples.end());
    int16_t* buf = m_boosted.data();
    for (size_t x = 0; x < m_boosted.size(); x++)
    {
        // Boost audio volume
        int sample = buf[x] * 1.80;
        // Hard clipping (audio compression)
        if (sample > INT16_MAX)
        {
            buf[x] = INT16_MAX;
            brls::Logger::debug("Audio Hard clipping INT16_MAX < {}", sample);
        }
        else if (sample < INT16_MIN)
        {
            buf[x] = INT16_MIN;
            brls::Logger::debug("Audio Hard clipping INT16_MIN > {}", sample);
        }
        else
        {
            buf[x] = (int16_t)sample;
        }
    }

    int audio_queued_size = SDL_GetQueuedAudioSize(m_device_id);
    if (audio_queued_size > 16000)
    {
        brls::Logger::warning("Triggering SDL_ClearQueuedAudio with queue size = {}", audio_queued_size);
        SDL_ClearQueuedAudio(m_device_id);
    }

    int success = SDL_QueueAudio(m_device_id, buf, sizeof(int16_t) * m_boosted.size());
    if (success != 0)
    {
        brls::Logger::error("SDL_QueueAudio failed: {}", SDL_GetError());
    }
}

void AudioManager::cleanup()
{
    if (m_device_id > 0)
    {
        SDL_CloseAudioDevice(m_device_id);
        m_device_id = 0;
    }
}
