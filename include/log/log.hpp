#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace akira::log {

enum class Level : std::uint8_t {
    error,
    warning,
    info,
    debug,
    verbose,
};

enum class Buffering : std::uint8_t {
    full,
    line,
};

using Clock = std::chrono::system_clock;

struct Entry {
    Clock::time_point time;
    Level level;
    std::string_view message;
};

class Log {
public:
    using Listener = std::function<void(const Entry&)>;

    class Subscription {
    public:
        Subscription(Subscription&& other) noexcept;
        Subscription& operator=(Subscription&& other) noexcept;
        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;
        ~Subscription();

    private:
        friend class Log;
        Subscription(Log& owner, std::uint64_t id) noexcept;
        void release() noexcept;

        Log* owner;
        std::uint64_t id;
    };

    class Redirect {
    public:
        Redirect(Redirect&& other) noexcept;
        Redirect& operator=(Redirect&& other) noexcept;
        Redirect(const Redirect&) = delete;
        Redirect& operator=(const Redirect&) = delete;
        ~Redirect();

    private:
        struct Closer {
            void operator()(std::FILE* file) const noexcept;
        };

        friend class Log;
        Redirect(Log& owner, std::uint64_t id, std::FILE* file) noexcept;
        void release() noexcept;

        Log* owner;
        std::uint64_t id;
        std::unique_ptr<std::FILE, Closer> file;
    };

    explicit Log(std::FILE* console) noexcept;
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    ~Log();

    static auto process() -> Log&;

    void setLevel(Level level) noexcept;
    void silence() noexcept;
    auto enabled(Level level) const noexcept -> bool;

    void setAsync(bool active) noexcept;
    void flush();

    void write(Level level, std::string message);

    [[nodiscard]] auto redirect(const std::string& path, Buffering buffering = Buffering::full)
        -> std::optional<Redirect>;
    auto subscribe(Listener listener) -> Subscription;

    static auto formatLine(Clock::time_point time, Level level, std::string_view message) -> std::string;

private:
    struct Target {
        std::uint64_t id;
        std::FILE* file;
    };

    struct Subscriber {
        std::uint64_t id;
        Listener listener;
    };

    static constexpr int silenced = -1;

    static auto label(Level level) noexcept -> std::string_view;
    static auto colour(Level level) noexcept -> std::string_view;

    auto output() const noexcept -> std::FILE*;
    void drainLocked();
    void release(const Redirect& redirect) noexcept;
    void release(const Subscription& subscription) noexcept;

    std::FILE* console;
    std::atomic<int> threshold{static_cast<int>(Level::info)};
    std::atomic<bool> async{false};
    std::atomic<std::uint64_t> nextId{1};

    mutable std::mutex outputMutex;
    std::vector<Target> targets;

    std::mutex queueMutex;
    std::deque<std::string> queue;

    std::mutex listenersMutex;
    std::vector<Subscriber> subscribers;
};

template <typename... Args>
void emit(Level level, std::format_string<Args...> format, Args&&... args)
{
    Log& sink = Log::process();
    if (sink.enabled(level))
        sink.write(level, std::format(format, std::forward<Args>(args)...));
}

template <typename... Args>
void error(std::format_string<Args...> format, Args&&... args)
{
    emit(Level::error, format, std::forward<Args>(args)...);
}

template <typename... Args>
void warning(std::format_string<Args...> format, Args&&... args)
{
    emit(Level::warning, format, std::forward<Args>(args)...);
}

template <typename... Args>
void info(std::format_string<Args...> format, Args&&... args)
{
    emit(Level::info, format, std::forward<Args>(args)...);
}

template <typename... Args>
void debug(std::format_string<Args...> format, Args&&... args)
{
    emit(Level::debug, format, std::forward<Args>(args)...);
}

template <typename... Args>
void verbose(std::format_string<Args...> format, Args&&... args)
{
    emit(Level::verbose, format, std::forward<Args>(args)...);
}

}
