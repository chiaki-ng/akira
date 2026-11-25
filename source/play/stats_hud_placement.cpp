#include "play/stats_hud_placement.hpp"

#include <algorithm>
#include <cmath>
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

auto StatsHudPlacement::fitted() const -> float
{
    if (panel.width <= 0.0f || panel.height <= 0.0f || screen.width <= 0.0f || screen.height <= 0.0f)
        return 1.0f;
    return std::min({ 1.0f, screen.width * widthShare / panel.width, screen.height * heightShare / panel.height });
}

auto StatsHudPlacement::drawn() const -> Size
{
    const float factor = fitted();
    return Size{ .width = panel.width * factor, .height = panel.height * factor };
}

auto StatsHudPlacement::scale() const -> float
{
    std::lock_guard guard(lock);
    return fitted();
}

void StatsHudPlacement::measureTarget(Tap tap, Area area)
{
    std::lock_guard guard(lock);
    targets[static_cast<std::size_t>(tap)] = area;
}

auto StatsHudPlacement::takeTapped() -> std::optional<Tap>
{
    std::lock_guard guard(lock);
    return std::exchange(tapped, std::nullopt);
}

auto StatsHudPlacement::target(Point finger) const -> std::optional<Tap>
{
    const Point corner = placed();
    const float factor = fitted();
    const float x = (finger.x - corner.x) / factor;
    const float y = (finger.y - corner.y) / factor;
    for (std::size_t index = 0; index < targets.size(); ++index)
    {
        const std::optional<Area>& area = targets[index];
        if (area && x >= area->x && x <= area->x + area->width && y >= area->y && y <= area->y + area->height)
            return static_cast<Tap>(index);
    }
    return std::nullopt;
}

auto StatsHudPlacement::placed() const -> Point
{
    const Size shown = drawn();
    const float x = saved ? saved->x * screen.width : margin;
    const float y = saved ? saved->y * screen.height : margin;
    return Point{ .x = std::clamp(x, margin, std::max(margin, screen.width - shown.width - margin)),
        .y = std::clamp(y, margin, std::max(margin, screen.height - shown.height - margin)) };
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
    const Size shown = drawn();
    if (finger.x < corner.x - slop || finger.x > corner.x + shown.width + slop || finger.y < corner.y - slop
        || finger.y > corner.y + shown.height + slop)
        return false;
    grabOffset = Point{ .x = finger.x - corner.x, .y = finger.y - corner.y };
    grabbedAt = finger;
    moved = false;
    dragging = true;
    return true;
}

void StatsHudPlacement::drag(Fraction at)
{
    std::lock_guard guard(lock);
    if (!dragging || screen.width <= 0.0f || screen.height <= 0.0f)
        return;
    const Point finger = touched(at);
    if (!moved && std::hypot(finger.x - grabbedAt.x, finger.y - grabbedAt.y) < slop)
        return;
    moved = true;
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
    if (!moved)
    {
        if (const std::optional<Tap> hit = target(grabbedAt))
        {
            tapped = hit;
            changed();
        }
        return;
    }
    const Point corner = placed();
    saved = Fraction{ .x = corner.x / screen.width, .y = corner.y / screen.height };
    settled = saved;
    changed();
}

}
