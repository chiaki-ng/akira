#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>

namespace akira {

class BackgroundWork {
public:
    using Listener = std::function<void(bool paused)>;

    class Pause {
    public:
        Pause();
        Pause(const Pause&) = delete;
        Pause& operator=(const Pause&) = delete;
        ~Pause();
    };

    class Subscription {
    public:
        Subscription() = default;
        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;
        Subscription(Subscription&& other) noexcept;
        Subscription& operator=(Subscription&& other) noexcept;
        ~Subscription();

    private:
        friend class BackgroundWork;
        explicit Subscription(uint64_t id) noexcept : id(id) {}

        uint64_t id = 0;
    };

    static auto paused() -> bool;
    [[nodiscard]] static auto subscribe(Listener listener) -> Subscription;

private:
    static void hold();
    static void release();
    static void announce(bool paused);
    static void unsubscribe(uint64_t id);

    static inline std::mutex mutex;
    static inline int holds = 0;
    static inline uint64_t nextId = 1;
    static inline std::map<uint64_t, Listener> listeners;
};

}
