#ifndef AKIRA_INPUT_CONTROLLER_TIMING_HPP
#define AKIRA_INPUT_CONTROLLER_TIMING_HPP

#include <algorithm>
#include <chrono>

namespace akira::platform::extended::timing {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

constexpr auto LegacyFramePeriod = std::chrono::nanoseconds(16'666'667);
constexpr auto ArrivalPollPeriod = LegacyFramePeriod * 30;
constexpr auto CouchClaimHold = LegacyFramePeriod * 40;
constexpr auto DisconnectGrace = LegacyFramePeriod * 30;

inline float progress(TimePoint startedAt, TimePoint now, Clock::duration duration)
{
    if (now <= startedAt)
        return 0.0f;
    const double elapsed = std::chrono::duration<double>(now - startedAt).count();
    const double total = std::chrono::duration<double>(duration).count();
    return static_cast<float>(std::clamp(elapsed / total, 0.0, 1.0));
}

} // namespace akira::platform::extended::timing

#endif // AKIRA_INPUT_CONTROLLER_TIMING_HPP
