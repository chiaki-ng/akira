#include "play/audio_worker.hpp"

#include "app/exception.hpp"

#include <utility>

namespace akira::play {

AudioWorker::AudioWorker(Play play, libchiaking::platform::ThreadingProvider& threading)
    : play(std::move(play))
{
    auto started = libchiaking::platform::Thread::start(threading, "audio",
        libchiaking::platform::ThreadRole::unspecified, [this](libchiaking::platform::StopToken) { run(); });
    if (!started)
        throw app::Exception("Failed to start the audio thread");
    thread = std::move(started).value();
}

AudioWorker::~AudioWorker()
{
    {
        std::lock_guard lock(mutex);
        stopping = true;
    }
    ready.notify_one();
    thread = libchiaking::platform::Thread{};
}

bool AudioWorker::submit(std::span<const std::byte> packet)
{
    bool kept = true;
    {
        std::lock_guard lock(mutex);
        if (packets.size() >= max_queued)
        {
            packets.pop_front();
            kept = false;
        }
        packets.emplace_back(packet.begin(), packet.end());
    }
    ready.notify_one();
    return kept;
}

void AudioWorker::run()
{
    while (true)
    {
        std::vector<std::byte> packet;
        {
            std::unique_lock lock(mutex);
            ready.wait(lock, [this] { return stopping || !packets.empty(); });
            if (stopping)
                return;
            packet = std::move(packets.front());
            packets.pop_front();
        }
        play(packet);
    }
}

}
