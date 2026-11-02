#include "util/http_pool.hpp"

#include <borealis.hpp>

#include "core/chiaki_core.hpp"


HttpPool& HttpPool::instance()
{
    static HttpPool* pool = new HttpPool();
    return *pool;
}

HttpPool::~HttpPool()
{
    stop();
}

void HttpPool::submit(Task task)
{
    if (!task)
        return;

    std::lock_guard<std::mutex> lock(mutex);
    if (stopping)
        return;

    ensureStarted();

    tasks.push_back(std::move(task));
    cond.notify_one();
}

void HttpPool::ensureStarted()
{
    if (!threads.empty())
        return;

    brls::Logger::info("HttpPool starting {} worker(s)", THREAD_COUNT);

    threads.reserve(THREAD_COUNT);
    for (int i = 0; i < THREAD_COUNT; i++)
    {
        auto started = libchiaking::platform::Thread::start(akira::ChiakiCore::current().threading(),
            WORKER_NAMES[static_cast<size_t>(i)], libchiaking::platform::ThreadRole::unspecified, [this, i]() { run(i); });
        if (!started)
        {
            brls::Logger::error("HttpPool worker {} could not start", i);
            continue;
        }
        threads.push_back(std::move(started).value());
    }
}

void HttpPool::stop()
{
    std::vector<libchiaking::platform::Thread> joining;

    {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopping)
            return;

        stopping = true;
        tasks.clear();
        cond.notify_all();
        joining.swap(threads);
    }

    joining.clear();
}

void HttpPool::run(int index)
{
    HttpSession session;

    while (true)
    {
        Task task;

        {
            std::unique_lock<std::mutex> lock(mutex);
            cond.wait(lock, [this]() { return stopping || !tasks.empty(); });

            if (stopping)
                break;

            task = std::move(tasks.front());
            tasks.pop_front();
        }

        task(session);
    }

    brls::Logger::info("HttpPool worker {} exiting", index);
}
