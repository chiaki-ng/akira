#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include <libchiaking/play_types.hpp>

#include "app/strings.hpp"

namespace akira::cloud {

class LaunchReport {
public:
    using Clock = std::chrono::steady_clock;

    static constexpr int steps = 4;

    void follow(const libchiaking::play::LaunchProgress& progress, Clock::time_point now);

    auto reported() const -> bool { return latest.has_value(); }
    auto step() const -> int { return reached; }
    auto status(const app::Strings& strings) const -> std::string;
    auto activity(const app::Strings& strings) const -> std::string;
    auto details(const app::Strings& strings, Clock::time_point now) const -> std::string;

private:
    static auto stepOf(libchiaking::play::LaunchPhase phase) -> int;
    static auto stepKey(int step) -> const char*;
    static auto activityKey(libchiaking::play::LaunchActivity activity) -> const char*;
    static auto waitKey(libchiaking::progress::WaitReason reason) -> const char*;
    static auto waitText(const app::Strings& strings, const libchiaking::progress::Wait& wait, std::int64_t since)
        -> std::string;
    static auto waitEstimate(const app::Strings& strings, std::uint32_t seconds) -> std::string;
    static auto rate(const app::Strings& strings, std::uint32_t kbps, std::optional<std::chrono::seconds> age)
        -> std::string;

    std::optional<libchiaking::play::LaunchProgress> latest;
    Clock::time_point received{};
    int reached = 1;
};

}
