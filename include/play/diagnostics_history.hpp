#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

#include "play/stream_stats.hpp"

namespace akira::play {

class DiagnosticsHistory {
public:
    enum class Event { NetworkLoss, RenderOverBudget, DecodeError };

    struct Interval {
        double seconds = 0.0;
        std::uint64_t framesLost = 0;
        std::uint64_t keyframeRequests = 0;
        std::uint64_t keyframeWaits = 0;
        std::uint64_t decodeErrors = 0;

        friend bool operator==(const Interval&, const Interval&) = default;
    };

    struct Frame {
        double bitrateMbps = 0.0;
        double rttMilliseconds = 0.0;
        double decodedFps = 0.0;
        double renderMilliseconds = 0.0;
        double framesLost = 0.0;
        Interval interval;
        std::vector<Event> events;

        friend bool operator==(const Frame&, const Frame&) = default;
    };

    static constexpr std::size_t capacity = 120;
    static constexpr std::size_t window = 20;
    static constexpr std::array events{ Event::NetworkLoss, Event::RenderOverBudget, Event::DecodeError };

    void record(const StreamStats& stats, double seconds);
    void reset();

    auto series(double Frame::* value) const -> std::vector<double>;
    auto recent(std::uint64_t Interval::* value) const -> std::uint64_t;
    auto marks(Event event) const -> std::vector<std::size_t>;
    auto empty() const -> bool { return frames.empty(); }

    friend bool operator==(const DiagnosticsHistory&, const DiagnosticsHistory&) = default;

private:
    static auto budgetMilliseconds(const StreamStats& stats) -> double;
    static auto decodeErrors(const StreamStats& stats) -> std::uint64_t;

    std::deque<Frame> frames;
    std::optional<StreamStats> previous;
};

}
