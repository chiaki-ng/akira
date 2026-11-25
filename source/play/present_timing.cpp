#include "play/present_timing.hpp"

#include <algorithm>
#include <cmath>
#include <format>

namespace akira::play {

void PresentTiming::Spread::add(double value)
{
    total += value;
    max = std::max(max, value);
    ++count;
}

auto PresentTiming::Spread::average() const -> double
{
    return count == 0 ? 0.0 : total / static_cast<double>(count);
}

auto PresentTiming::milliseconds(Clock::duration span) -> double
{
    return std::chrono::duration<double, std::milli>(span).count();
}

auto PresentTiming::percentile(std::vector<double> samples, double fraction) -> double
{
    if (samples.empty())
        return 0.0;
    const auto rank = static_cast<std::size_t>(std::ceil(fraction * static_cast<double>(samples.size()))) - 1;
    const auto at = samples.begin() + static_cast<std::ptrdiff_t>(std::min(rank, samples.size() - 1));
    std::ranges::nth_element(samples, at);
    return *at;
}

void PresentTiming::presented(Kind kind, Stamps stamps)
{
    Totals* counted[] = { &overall, &windowed };
    for (Totals* totals : counted)
        ++(kind == Kind::frame ? totals->frames : totals->repaints);
    if (!started)
        started = stamps.presented;
    if (lastPresent)
        intervals.push_back(milliseconds(stamps.presented - *lastPresent));
    lastPresent = stamps.presented;
    readyToSubmit.add(milliseconds(stamps.submitted - stamps.ready));
    submitToPresent.add(milliseconds(stamps.presented - stamps.submitted));
}

void PresentTiming::missed(Miss miss)
{
    Totals* counted[] = { &overall, &windowed };
    for (Totals* totals : counted)
        ++(miss == Miss::busy ? totals->busy : totals->unavailable);
}

void PresentTiming::copied(std::size_t bytes)
{
    Totals* counted[] = { &overall, &windowed };
    for (Totals* totals : counted)
    {
        ++totals->copies;
        totals->copiedBytes += bytes;
    }
}

void PresentTiming::swapInterval(std::uint32_t value)
{
    interval = value;
}

void PresentTiming::restart(Clock::time_point now)
{
    windowed = Totals{};
    started = now;
    intervals.clear();
    readyToSubmit = Spread{};
    submitToPresent = Spread{};
}

auto PresentTiming::report(Clock::time_point now) -> std::optional<std::string>
{
    if (!started || now - *started < window)
        return std::nullopt;
    const double elapsed = milliseconds(now - *started);
    const double average = intervals.empty()
        ? 0.0
        : std::ranges::fold_left(intervals, 0.0, std::plus<>{}) / static_cast<double>(intervals.size());
    const double longest = intervals.empty() ? 0.0 : std::ranges::max(intervals);
    std::string line = std::format(
        "[PRESENT] {} frames + {} repaints in {:.0f} ms, swap {}: interval avg {:.2f} p95 {:.2f} max {:.2f} ms; "
        "ready->submit avg {:.2f} max {:.2f} ms; submit->present avg {:.3f} max {:.3f} ms; "
        "skipped busy {} unavailable {}; copies {} ({} B)",
        windowed.frames, windowed.repaints, elapsed, interval, average, percentile(intervals, 0.95), longest,
        readyToSubmit.average(), readyToSubmit.max, submitToPresent.average(), submitToPresent.max, windowed.busy,
        windowed.unavailable, windowed.copies, windowed.copiedBytes);
    restart(now);
    return line;
}

}
