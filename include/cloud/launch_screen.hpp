#ifndef AKIRA_CLOUD_LAUNCH_SCREEN_HPP
#define AKIRA_CLOUD_LAUNCH_SCREEN_HPP

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <libchiaking/play_types.hpp>

#include "cloud/start_steps.hpp"

struct NVGcontext;

namespace cloud {

class LaunchScreen {
public:
    explicit LaunchScreen(std::string title);

    void progress(const libchiaking::play::LaunchProgress& progress);
    void playing();
    void failed();
    void draw(NVGcontext* vg, float x, float y, float width, float height) const;

private:
    using Clock = std::chrono::steady_clock;

    auto status() const -> std::string;
    auto operationTitle() const -> std::optional<std::string>;
    auto operationDetails(int since) const -> std::string;
    auto sinceReport() const -> int;

    static auto stepOf(libchiaking::play::LaunchPhase phase) -> StartSteps::Step;
    static auto stepLabel(StartSteps::Step step) -> std::string;
    static auto activityLabel(libchiaking::play::LaunchActivity activity) -> std::string;
    static auto waitLabel(libchiaking::progress::WaitReason reason) -> std::string;
    static auto waitText(const libchiaking::progress::Wait& wait, int since) -> std::string;
    static auto waitEstimate(uint32_t seconds) -> std::string;

    std::string title;
    StartSteps steps;
    std::optional<libchiaking::play::LaunchProgress> latest;
    Clock::time_point received{};
};

} // namespace cloud

#endif // AKIRA_CLOUD_LAUNCH_SCREEN_HPP
