#ifndef AKIRA_IO_AUDIO_MANAGER_HPP
#define AKIRA_IO_AUDIO_MANAGER_HPP

#include <SDL2/SDL.h>
#include <cstdint>
#include <span>
#include <vector>

class AudioManager
{
public:
    AudioManager();
    ~AudioManager();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;


    void init(unsigned int channels, unsigned int rate);
    void play(std::span<const int16_t> samples);
    void cleanup();

    bool isInitialized() const { return m_device_id > 0; }

private:
    unsigned int m_channels = 0;
    std::vector<int16_t> m_boosted;
    SDL_AudioDeviceID m_device_id = 0;
};

#endif // AKIRA_IO_AUDIO_MANAGER_HPP
