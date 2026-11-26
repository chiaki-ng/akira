#ifndef AKIRA_AUDIO_WORKER_HPP
#define AKIRA_AUDIO_WORKER_HPP

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <span>
#include <vector>

#include <libchiaking/platform/platform.hpp>

namespace akira::play {

class AudioWorker {
public:
    using Play = std::function<void(std::span<const std::byte> packet)>;

    AudioWorker(Play play, libchiaking::platform::ThreadingProvider& threading);
    AudioWorker(const AudioWorker&) = delete;
    AudioWorker& operator=(const AudioWorker&) = delete;
    ~AudioWorker();

    bool submit(std::span<const std::byte> packet);

private:
    static constexpr std::size_t max_queued = 16;

    void run();

    Play play;
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<std::vector<std::byte>> packets;
    bool stopping = false;
    libchiaking::platform::Thread thread;
};

}

#endif
