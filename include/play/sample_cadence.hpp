#ifndef AKIRA_INPUT_SAMPLE_CADENCE_HPP
#define AKIRA_INPUT_SAMPLE_CADENCE_HPP

#include <chrono>

namespace akira::input {

constexpr auto InputPollPeriod = std::chrono::nanoseconds(8'333'333);
constexpr auto MotionPollPeriod = std::chrono::nanoseconds(16'666'666);
constexpr auto DirectDiscoveryPeriod = std::chrono::milliseconds(500);

class SampleCadence
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    explicit constexpr SampleCadence(Clock::duration period)
        : m_period(period)
    {
    }

    bool due(TimePoint now)
    {
        if (!m_started)
        {
            m_started = true;
            m_next = now + m_period;
            return true;
        }

        if (now < m_next)
            return false;

        m_next += m_period;
        if (now >= m_next)
            m_next = now + m_period;
        return true;
    }

private:
    Clock::duration m_period;
    TimePoint m_next{};
    bool m_started = false;
};

} // namespace akira::input

#endif // AKIRA_INPUT_SAMPLE_CADENCE_HPP
