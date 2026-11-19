#pragma once

#include "app/views/theme.hpp"

namespace akira::ui
{
inline float clampGlassAlpha(float value)
{
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

inline NVGcolor fadeGlassColor(NVGcolor color, float alpha)
{
    color.a *= clampGlassAlpha(alpha);
    return color;
}

inline void drawGlassSurface(NVGcontext* vg, float x, float y, float width, float height,
                             float radius, float alpha, bool elevated,
                             const Palette& palette, float scale = 1.0f)
{
    const float a = clampGlassAlpha(alpha);
    const float s = scale > 0.0f ? scale : 1.0f;

    NVGpaint shadow = nvgBoxGradient(vg, x - 6.0f * s, y - 4.0f * s,
                                     width + 12.0f * s, height + 14.0f * s,
                                     radius + 5.0f * s, 18.0f * s,
                                     fadeGlassColor(nvgRGBA(0, 0, 0, 105), a),
                                     nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x - 18.0f * s, y - 16.0f * s,
                   width + 36.0f * s, height + 42.0f * s, radius + 16.0f * s);
    nvgFillPaint(vg, shadow);
    nvgFill(vg);

    const NVGcolor top = fadeGlassColor(withAlpha(
        elevated ? palette.surfaceElevated : palette.surface,
        elevated ? 0x8a : 0x70), a);
    const NVGcolor bottom = fadeGlassColor(withAlpha(
        palette.backgroundDeep, elevated ? 0xc0 : 0xa4), a);
    NVGpaint glass = nvgLinearGradient(vg, x, y, x, y + height, top, bottom);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, radius);
    nvgFillPaint(vg, glass);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 0.5f * s, y + 0.5f * s,
                   width - 1.0f * s, height - 1.0f * s, radius);
    nvgStrokeColor(vg, fadeGlassColor(elevated
        ? withAlpha(palette.accent, 0x9c)
        : withAlpha(palette.focusB, 0x30), a));
    nvgStrokeWidth(vg, 1.0f * s);
    nvgStroke(vg);

    nvgBeginPath(vg);
    nvgMoveTo(vg, x + radius, y + 1.5f * s);
    nvgLineTo(vg, x + width - radius, y + 1.5f * s);
    nvgStrokeColor(vg, fadeGlassColor(withAlpha(palette.focusB, 0x48), a));
    nvgStrokeWidth(vg, 1.0f * s);
    nvgLineCap(vg, NVG_ROUND);
    nvgStroke(vg);
}
}
