#include "stream/video_worker.hpp"

#include "core/chiaki_core.hpp"
#include "core/exception.hpp"

#include <algorithm>
#include <utility>

namespace akira::stream {

VideoWorker::VideoWorker(Decode decode)
    : decode(std::move(decode))
{
    auto started = libchiaking::platform::Thread::start(akira::ChiakiCore::current().threading(), "video",
        libchiaking::platform::ThreadRole::feedback, [this](libchiaking::platform::StopToken) { run(); });
    if (!started)
        throw Exception("Failed to start the video thread");
    thread = std::move(started).value();
}

VideoWorker::~VideoWorker()
{
    {
        std::lock_guard lock(mutex);
        stopping = true;
    }
    ready.notify_one();
    thread = libchiaking::platform::Thread{};
}

bool VideoWorker::submit(std::span<const std::byte> header, std::span<const std::byte> frame)
{
    const std::size_t size = header.size() + frame.size();
    AVBufferHandle packet(size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!packet)
        return false;
    std::ranges::copy(header, packet.data());
    std::ranges::copy(frame, packet.data() + header.size());
    std::fill_n(packet.data() + size, AV_INPUT_BUFFER_PADDING_SIZE, std::byte{0});

    bool kept = true;
    {
        std::lock_guard lock(mutex);
        if (jobs.size() >= max_queued)
        {
            jobs.clear();
            kept = false;
        }
        jobs.push_back(Job{std::move(packet), size});
    }
    ready.notify_one();
    return kept;
}

void VideoWorker::run()
{
    while (true)
    {
        Job job;
        {
            std::unique_lock lock(mutex);
            ready.wait(lock, [this] { return stopping || !jobs.empty(); });
            if (stopping)
                return;
            job = std::move(jobs.front());
            jobs.pop_front();
        }
        decode(job.packet, job.size);
    }
}

}
