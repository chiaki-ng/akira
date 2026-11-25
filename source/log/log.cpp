#include "log/log.hpp"

#include <algorithm>
#include <ctime>
#include <exception>

namespace akira::log {

Log::Subscription::Subscription(Log& owner, std::uint64_t id) noexcept
    : owner(&owner)
    , id(id)
{
}

Log::Subscription::Subscription(Subscription&& other) noexcept
    : owner(std::exchange(other.owner, nullptr))
    , id(other.id)
{
}

Log::Subscription& Log::Subscription::operator=(Subscription&& other) noexcept
{
    if (this != &other)
    {
        release();
        owner = std::exchange(other.owner, nullptr);
        id = other.id;
    }
    return *this;
}

Log::Subscription::~Subscription()
{
    release();
}

void Log::Subscription::release() noexcept
{
    if (Log* log = std::exchange(owner, nullptr))
        log->release(*this);
}

void Log::Redirect::Closer::operator()(std::FILE* file) const noexcept
{
    std::fclose(file);
}

Log::Redirect::Redirect(Log& owner, std::uint64_t id, std::FILE* file) noexcept
    : owner(&owner)
    , id(id)
    , file(file)
{
}

Log::Redirect::Redirect(Redirect&& other) noexcept
    : owner(std::exchange(other.owner, nullptr))
    , id(other.id)
    , file(std::move(other.file))
{
}

Log::Redirect& Log::Redirect::operator=(Redirect&& other) noexcept
{
    if (this != &other)
    {
        release();
        owner = std::exchange(other.owner, nullptr);
        id = other.id;
        file = std::move(other.file);
    }
    return *this;
}

Log::Redirect::~Redirect()
{
    release();
}

void Log::Redirect::release() noexcept
{
    if (Log* log = std::exchange(owner, nullptr))
        log->release(*this);
    file.reset();
}

Log::Log(std::FILE* console) noexcept
    : console(console)
{
}

Log::~Log()
{
    try {
        flush();
    } catch (...) {
    }
}

auto Log::process() -> Log&
{
    static Log instance{stdout};
    return instance;
}

void Log::setLevel(Level level) noexcept
{
    threshold.store(static_cast<int>(level), std::memory_order_relaxed);
}

void Log::silence() noexcept
{
    threshold.store(silenced, std::memory_order_relaxed);
}

auto Log::enabled(Level level) const noexcept -> bool
{
    return static_cast<int>(level) <= threshold.load(std::memory_order_relaxed);
}

void Log::setAsync(bool active) noexcept
{
    async.store(active, std::memory_order_relaxed);
}

void Log::flush()
{
    std::lock_guard lock(outputMutex);
    drainLocked();
}

void Log::write(Level level, std::string message)
{
    const Clock::time_point now = Clock::now();
    std::string line = formatLine(now, level, message);

    if (async.load(std::memory_order_relaxed))
    {
        std::lock_guard lock(queueMutex);
        queue.push_back(std::move(line));
    }
    else
    {
        std::lock_guard lock(outputMutex);
        std::fwrite(line.data(), 1, line.size(), output());
    }

    std::lock_guard lock(listenersMutex);
    const Entry entry{now, level, message};
    for (const Subscriber& subscriber : subscribers)
    {
        try {
            subscriber.listener(entry);
        } catch (const std::exception&) {
        }
    }
}

auto Log::redirect(const std::string& path, Buffering buffering) -> std::optional<Redirect>
{
    std::FILE* file = std::fopen(path.c_str(), "w");
    if (!file)
        return std::nullopt;
    if (buffering == Buffering::line)
        std::setvbuf(file, nullptr, _IOLBF, 0);

    const std::uint64_t id = nextId.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard lock(outputMutex);
    drainLocked();
    targets.push_back({id, file});
    return Redirect{*this, id, file};
}

auto Log::subscribe(Listener listener) -> Subscription
{
    const std::uint64_t id = nextId.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard lock(listenersMutex);
    subscribers.push_back({id, std::move(listener)});
    return Subscription{*this, id};
}

auto Log::formatLine(Clock::time_point time, Level level, std::string_view message) -> std::string
{
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count() % 1000;
    const std::time_t seconds = Clock::to_time_t(time);
    std::tm local{};
    localtime_r(&seconds, &local);
    return std::format("{:02}:{:02}:{:02}.{:03}\033{}[{}]\033[0m {}\n",
        local.tm_hour, local.tm_min, local.tm_sec, static_cast<int>(ms), colour(level), label(level), message);
}

auto Log::label(Level level) noexcept -> std::string_view
{
    switch (level)
    {
        case Level::error: return "ERROR";
        case Level::warning: return "WARNING";
        case Level::info: return "INFO";
        case Level::debug: return "DEBUG";
        case Level::verbose: return "VERBOSE";
    }
    return "INFO";
}

auto Log::colour(Level level) noexcept -> std::string_view
{
    switch (level)
    {
        case Level::error: return "[0;31m";
        case Level::warning: return "[0;33m";
        case Level::info: return "[0;34m";
        case Level::debug: return "[0;32m";
        case Level::verbose: return "[0;37m";
    }
    return "[0;34m";
}

auto Log::output() const noexcept -> std::FILE*
{
    return targets.empty() ? console : targets.back().file;
}

void Log::drainLocked()
{
    std::deque<std::string> batch;
    {
        std::lock_guard lock(queueMutex);
        batch.swap(queue);
    }
    std::FILE* out = output();
    for (const std::string& line : batch)
        std::fwrite(line.data(), 1, line.size(), out);
    std::fflush(out);
}

void Log::release(const Redirect& redirect) noexcept
{
    std::lock_guard lock(outputMutex);
    try {
        drainLocked();
    } catch (...) {
    }
    std::erase_if(targets, [&](const Target& target) { return target.id == redirect.id; });
}

void Log::release(const Subscription& subscription) noexcept
{
    std::lock_guard lock(listenersMutex);
    std::erase_if(subscribers, [&](const Subscriber& subscriber) { return subscriber.id == subscription.id; });
}

}
