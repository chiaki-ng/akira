#include "play/stats_hud_placement.hpp"

#include <algorithm>
#include <utility>

namespace akira::play {

StatsHudPlacement::StatsHudPlacement(std::optional<Fraction> saved)
    : saved(saved)
{
}

void StatsHudPlacement::changed()
{
    ++revisions;
}

void StatsHudPlacement::show(bool visible)
{
    std::lock_guard guard(lock);
    if (shown == visible)
        return;
    shown = visible;
    if (!shown)
        dragging = false;
    changed();
}

void StatsHudPlacement::restore(std::optional<Fraction> position)
{
    std::lock_guard guard(lock);
    if (saved == position)
        return;
    saved = position;
    changed();
}

void StatsHudPlacement::measureScreen(Size measured)
{
    std::lock_guard guard(lock);
    if (screen == measured)
        return;
    screen = measured;
    changed();
}

void StatsHudPlacement::measurePanel(Size measured)
{
    std::lock_guard guard(lock);
    if (panel == measured)
        return;
    panel = measured;
    changed();
}

auto StatsHudPlacement::placed() const -> Point
{
    const float x = saved ? saved->x * screen.width : margin;
    const float y = saved ? saved->y * screen.height : margin;
    return Point{ .x = std::clamp(x, margin, std::max(margin, screen.width - panel.width - margin)),
        .y = std::clamp(y, margin, std::max(margin, screen.height - panel.height - margin)) };
}

auto StatsHudPlacement::touched(Fraction at) const -> Point
{
    return Point{ .x = at.x * screen.width, .y = at.y * screen.height };
}

auto StatsHudPlacement::origin() const -> Point
{
    std::lock_guard guard(lock);
    return placed();
}

auto StatsHudPlacement::revision() const -> std::uint64_t
{
    std::lock_guard guard(lock);
    return revisions;
}

auto StatsHudPlacement::takeSettled() -> std::optional<Fraction>
{
    std::lock_guard guard(lock);
    return std::exchange(settled, std::nullopt);
}

auto StatsHudPlacement::grab(Fraction at) -> bool
{
    std::lock_guard guard(lock);
    if (!shown || panel.width <= 0.0f || panel.height <= 0.0f || screen.width <= 0.0f || screen.height <= 0.0f)
        return false;
    const Point corner = placed();
    const Point finger = touched(at);
    if (finger.x < corner.x - slop || finger.x > corner.x + panel.width + slop || finger.y < corner.y - slop
        || finger.y > corner.y + panel.height + slop)
        return false;
    grabOffset = Point{ .x = finger.x - corner.x, .y = finger.y - corner.y };
    dragging = true;
    return true;
}

void StatsHudPlacement::drag(Fraction at)
{
    std::lock_guard guard(lock);
    if (!dragging || screen.width <= 0.0f || screen.height <= 0.0f)
        return;
    const Point finger = touched(at);
    saved = Fraction{ .x = (finger.x - grabOffset.x) / screen.width, .y = (finger.y - grabOffset.y) / screen.height };
    changed();
}

void StatsHudPlacement::release()
{
    std::lock_guard guard(lock);
    if (!dragging)
        return;
    dragging = false;
    if (screen.width <= 0.0f || screen.height <= 0.0f)
        return;
    const Point corner = placed();
    saved = Fraction{ .x = corner.x / screen.width, .y = corner.y / screen.height };
    settled = saved;
    changed();
}

}
