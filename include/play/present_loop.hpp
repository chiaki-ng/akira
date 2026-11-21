#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <expected>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>

#include "play/present_timing.hpp"

namespace akira::play {

class PresentLoop {
public:
    using Clock = PresentTiming::Clock;

    struct Pacing {
        std::uint32_t liveInterval = 0;
        std::uint32_t pausedInterval = 1;
        std::chrono::milliseconds pausedRepaint{ 16 };
        std::chrono::milliseconds stallRepaint{ 250 };
    };

    struct Presented {
        Clock::time_point submitted;
        Clock::time_point presented;
    };

    class Target {
    public:
        virtual ~Target() = default;

        [[nodiscard]] virtual auto present(PresentTiming::Kind kind, PresentTiming& timing)
            -> std::expected<Presented, PresentTiming::Miss> = 0;
        virtual void swapInterval(std::uint32_t interval) = 0;
    };

    PresentLoop(Target& target, Pacing pacing);
    ~PresentLoop();

    PresentLoop(const PresentLoop&) = delete;
    PresentLoop& operator=(const PresentLoop&) = delete;

    void frameReady();
    void paused(bool paused);
    void serialized(std::function<void()> work);
    void stop();
    auto totals() const -> PresentTiming::Totals;

private:
    void present(PresentTiming::Kind kind, std::unique_lock<std::mutex>& held);
    void repaint(std::stop_token stop);
    auto quiet() const -> std::chrono::milliseconds;

    Target& target;
    Pacing pacing;
    mutable std::mutex mutex;
    std::condition_variable_any wake;
    PresentTiming timing;
    std::optional<std::uint32_t> appliedInterval;
    std::optional<Clock::time_point> lastPresent;
    std::atomic<bool> held{ false };
    std::atomic<std::uint64_t> nudges{ 0 };
    std::atomic<std::thread::id> presenting{};
    std::jthread repainter;
};

}
