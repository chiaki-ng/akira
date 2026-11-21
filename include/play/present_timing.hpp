#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace akira::play {

class PresentTiming {
public:
    using Clock = std::chrono::steady_clock;

    static constexpr std::chrono::milliseconds window{ 1000 };

    struct Stamps {
        Clock::time_point ready;
        Clock::time_point submitted;
        Clock::time_point presented;
    };

    enum class Kind {
        frame,
        repaint,
    };

    enum class Miss {
        busy,
        unavailable,
    };

    struct Totals {
        std::uint64_t frames = 0;
        std::uint64_t repaints = 0;
        std::uint64_t busy = 0;
        std::uint64_t unavailable = 0;
        std::uint64_t copies = 0;
        std::uint64_t copiedBytes = 0;

        friend bool operator==(const Totals&, const Totals&) = default;
    };

    void presented(Kind kind, Stamps stamps);
    void missed(Miss miss);
    void copied(std::size_t bytes);
    void swapInterval(std::uint32_t interval);

    [[nodiscard]] auto report(Clock::time_point now) -> std::optional<std::string>;
    auto totals() const -> Totals { return overall; }

private:
    struct Spread {
        double total = 0.0;
        double max = 0.0;
        std::uint64_t count = 0;

        void add(double milliseconds);
        auto average() const -> double;
    };

    static auto milliseconds(Clock::duration span) -> double;
    static auto percentile(std::vector<double> samples, double fraction) -> double;
    void restart(Clock::time_point now);

    Totals overall;
    Totals windowed;
    std::optional<Clock::time_point> started;
    std::optional<Clock::time_point> lastPresent;
    std::vector<double> intervals;
    Spread readyToSubmit;
    Spread submitToPresent;
    std::uint32_t interval = 1;
};

}
