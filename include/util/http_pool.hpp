#ifndef AKIRA_HTTP_POOL_HPP
#define AKIRA_HTTP_POOL_HPP

#include <array>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string_view>
#include <vector>

#include <libchiaking/platform/platform.hpp>

#include "util/http.hpp"

class HttpPool {
public:
    using Task = std::function<void(HttpSession&)>;

    static HttpPool& instance();

    void submit(Task task);
    void stop();

    size_t threadCount() const { return threads.size(); }

private:
    HttpPool() = default;
    ~HttpPool();

    HttpPool(const HttpPool&) = delete;
    HttpPool& operator=(const HttpPool&) = delete;

    void ensureStarted();
    void run(int index);

    static constexpr int THREAD_COUNT = 4;
    static constexpr std::array<std::string_view, THREAD_COUNT> WORKER_NAMES{"http-0", "http-1", "http-2", "http-3"};

    std::vector<libchiaking::platform::Thread> threads;
    std::deque<Task> tasks;
    mutable std::mutex mutex;
    std::condition_variable cond;
    bool stopping = false;
};

#endif // AKIRA_HTTP_POOL_HPP
