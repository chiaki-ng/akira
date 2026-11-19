#include "play/views/progress_ring.hpp"
#include "app/views/theme.hpp"

#include <algorithm>

ProgressRing::ProgressRing()
{
    this->setAxis(brls::Axis::COLUMN);
    this->setJustifyContent(brls::JustifyContent::CENTER);
    this->setAlignItems(brls::AlignItems::CENTER);
}

void ProgressRing::setProgress(float ratio)
{
    float target = std::clamp(ratio, 0.0f, 1.0f);
    if (target == progress)
        return;
    progress = target;
    displayProgress.reset(displayProgress.getValue());
    displayProgress.addStep(target, 520, brls::EasingFunction::quadraticOut);
    displayProgress.start();
}

void ProgressRing::setThickness(float px)
{
    thickness = px;
}

void ProgressRing::draw(NVGcontext* vg, float x, float y, float width, float height,
    brls::Style style, brls::FrameContext* ctx)
{
    float cx = x + width / 2.0f;
    float cy = y + height / 2.0f;
    float radius = std::min(width, height) / 2.0f - thickness / 2.0f;

    nvgLineCap(vg, NVG_ROUND);
    nvgStrokeWidth(vg, thickness);

    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, radius);
    nvgStrokeColor(vg, a(akira::app::views::withAlpha(akira::app::views::active().text, 0x1f)));
    nvgStroke(vg);

    float shown = displayProgress.getValue();
    if (shown > 0.0f)
    {
        float start = -NVG_PI / 2.0f;
        float end = start + shown * 2.0f * NVG_PI;

        nvgBeginPath(vg);
        nvgArc(vg, cx, cy, radius, start, end, NVG_CW);
        nvgStrokeColor(vg, a(akira::app::views::active().accent));
        nvgStroke(vg);
    }

    Box::draw(vg, x, y, width, height, style, ctx);
}
