#ifndef AKIRA_INPUT_GESTURE_TIMING_HPP
#define AKIRA_INPUT_GESTURE_TIMING_HPP

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace akira::input::gesture {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

constexpr auto LegacyFramePeriod = std::chrono::nanoseconds(16'666'667);
constexpr auto SwipeDuration = LegacyFramePeriod * 18;
constexpr auto BorderTapCommitDelay = LegacyFramePeriod * 4;
constexpr auto TouchpadButtonDelay = LegacyFramePeriod * 4;
constexpr auto TouchpadButtonHold = LegacyFramePeriod * 12;
constexpr auto TouchWarningInterval = LegacyFramePeriod * 300;

inline int16_t interpolate(int16_t start, int16_t end,
    Clock::duration elapsed, Clock::duration duration)
{
    if (elapsed <= Clock::duration::zero())
        return start;
    if (elapsed >= duration)
        return end;

    const double progress = std::chrono::duration<double>(elapsed).count()
        / std::chrono::duration<double>(duration).count();
    const double value = static_cast<double>(start)
        + static_cast<double>(end - start) * progress;
    return static_cast<int16_t>(std::clamp(value,
        static_cast<double>(std::min(start, end)),
        static_cast<double>(std::max(start, end))));
}

} // namespace akira::input::gesture

#endif // AKIRA_INPUT_GESTURE_TIMING_HPP
