#ifndef AKIRA_VIDEO_WORKER_HPP
#define AKIRA_VIDEO_WORKER_HPP

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <span>
#include <vector>

#include <libchiaking/platform/platform.hpp>

#include "util/av_wrappers.hpp"

namespace akira::stream {

class VideoWorker {
public:
    using Decode = std::function<void(const AVBufferHandle& packet, std::size_t size)>;

    explicit VideoWorker(Decode decode);
    VideoWorker(const VideoWorker&) = delete;
    VideoWorker& operator=(const VideoWorker&) = delete;
    ~VideoWorker();

    bool submit(std::span<const std::byte> header, std::span<const std::byte> frame);

private:
    struct Job {
        AVBufferHandle packet;
        std::size_t size = 0;
    };

    static constexpr std::size_t max_queued = 8;

    void run();

    Decode decode;
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<Job> jobs;
    bool stopping = false;
    libchiaking::platform::Thread thread;
};

}

#endif
