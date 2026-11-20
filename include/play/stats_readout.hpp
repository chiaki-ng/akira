#pragma once

#include <optional>
#include <string>
#include <vector>

#include "play/stream_stats.hpp"
#include "play/upscaling_summary.hpp"

namespace akira::play {

class StatsReadout {
public:
    enum class Tone { Good, Warn, Bad };

    struct Cell {
        std::string label;
        std::string value;
        std::string unit;
        Tone tone = Tone::Good;

        friend bool operator==(const Cell&, const Cell&) = default;
    };

    struct Sample {
        StreamStats stats;
        UpscalingSummary upscaling;
        std::string decoder;
        std::string context;
        std::optional<std::string> tunnel;

        friend bool operator==(const Sample&, const Sample&) = default;
    };

    struct Bar {
        float filled = 0.0f;
        float network = 0.0f;

        friend bool operator==(const Bar&, const Bar&) = default;
    };

    static constexpr float latencyReferenceMs = 120.0f;

    StatsReadout() = default;
    explicit StatsReadout(Sample sample);

    auto compact() const -> std::vector<Cell>;
    auto overall() const -> Tone;
    auto upscaled() const -> bool { return sample.upscaling.active; }
    auto upscaleSummary() const -> std::string;
    auto upscalePath() const -> std::string;
    auto upscaleDetail() const -> std::string;
    auto context() const -> std::string;
    auto uptime() const -> std::string;
    auto requested() const -> std::vector<Cell>;
    auto output() const -> std::vector<Cell>;
    auto link() const -> std::vector<Cell>;
    auto latency() const -> Cell;
    auto bar() const -> Bar;
    auto legend() const -> std::vector<Cell>;
    auto footer() const -> std::vector<Cell>;

    friend bool operator==(const StatsReadout&, const StatsReadout&) = default;

private:
    static auto codec(bool hevc) -> std::string;
    static auto graded(float value, float good, float fair) -> Tone;
    static auto gradedShare(float share, float good, float fair) -> Tone;
    auto fps() const -> std::string;
    auto rate() const -> std::string;
    auto loss() const -> std::string;
    auto rtt() const -> std::string;
    auto totalLatency() const -> std::string;
    auto fpsTone() const -> Tone;
    auto rateTone() const -> Tone;
    auto lossTone() const -> Tone;
    auto rttTone() const -> Tone;
    auto lostTone() const -> Tone;
    auto latencyTone() const -> Tone;
    auto rcasStrength() const -> int;

    Sample sample;
};

}
