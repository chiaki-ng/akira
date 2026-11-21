#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "play/diagnostics_history.hpp"
#include "play/stream_stats.hpp"
#include "play/upscaling_summary.hpp"

namespace akira::play {

class DiagnosticsReport {
public:
    enum class Tone { Neutral, Good, Warning, Bad };
    enum class StageKind { Network, Assembly, Decode, Render, Display };

    struct Row {
        std::string label;
        std::string value;
        Tone tone = Tone::Neutral;
        bool keyed = false;

        friend bool operator==(const Row&, const Row&) = default;
    };

    struct Stage {
        StageKind kind = StageKind::Network;
        std::string headline;
        std::string unit;
        Tone tone = Tone::Neutral;
        std::vector<double> series;
        std::vector<Row> rows;

        friend bool operator==(const Stage&, const Stage&) = default;
    };

    struct Section {
        std::string title;
        std::vector<Row> rows;

        friend bool operator==(const Section&, const Section&) = default;
    };

    struct Chip {
        std::vector<std::string> segments;
        Tone tone = Tone::Neutral;

        friend bool operator==(const Chip&, const Chip&) = default;
    };

    struct Counter {
        std::string label;
        std::uint64_t value = 0;

        friend bool operator==(const Counter&, const Counter&) = default;
    };

    struct Alert {
        StageKind kind = StageKind::Network;
        std::string label;
        std::string value;
        Tone tone = Tone::Warning;
        bool headline = false;

        friend bool operator==(const Alert&, const Alert&) = default;
    };

    struct Sample {
        StreamStats stats;
        UpscalingSummary upscaling;
        std::string decoder;
        std::string context;

        friend bool operator==(const Sample&, const Sample&) = default;
    };

    static constexpr const char* unavailable = "\xe2\x80\x93";

    DiagnosticsReport() = default;
    DiagnosticsReport(Sample sample, DiagnosticsHistory history);

    auto stages() const -> std::vector<Stage>;
    auto overallTone() const -> Tone;
    auto compactTone() const -> Tone;
    auto compactAlert() const -> std::optional<Alert>;
    auto headerChips() const -> std::vector<Chip>;
    auto fullSections() const -> std::vector<Section>;
    auto counters() const -> std::vector<Counter>;
    auto events() const -> std::vector<DiagnosticsHistory::Event>;
    auto marks(DiagnosticsHistory::Event event) const -> std::vector<std::size_t> { return history.marks(event); }
    auto uptime() const -> std::string;
    auto rttValue() const -> std::string;
    auto rttPresent() const -> bool { return sample.stats.rtt_valid; }
    auto lossValue() const -> std::string;
    auto lossTone() const -> Tone;
    auto bitrateValue() const -> std::string;
    auto latencySummary() const -> std::string;

    friend bool operator==(const DiagnosticsReport&, const DiagnosticsReport&) = default;

private:
    auto networkStage() const -> Stage;
    auto assemblyStage() const -> Stage;
    auto decodeStage() const -> Stage;
    auto renderStage() const -> Stage;
    auto displayStage() const -> Stage;
    auto networkSection() const -> Section;
    auto framesSection() const -> Section;
    auto presentationSection() const -> Section;
    auto latencySection() const -> Section;
    auto budgetMilliseconds() const -> double;
    auto upscaler() const -> std::optional<std::string>;
    static auto worst(const std::vector<Stage>& shown) -> Tone;
    static auto lossToneOf(double percent) -> Tone;
    static auto countTone(std::uint64_t count, bool bad = false) -> Tone;
    static auto ratioTone(double value, double target) -> Tone;
    static auto oneDecimal(double value) -> std::string;
    static auto millisecondsNumber(double value) -> std::string;
    static auto milliseconds(double value) -> std::string;
    static auto resolution(int width, int height) -> std::string;

    Sample sample;
    DiagnosticsHistory history;
};

}
