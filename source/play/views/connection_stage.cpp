#include "play/views/connection_stage.hpp"
#include "app/views/theme.hpp"

#include <borealis.hpp>

#include <cmath>

namespace akira::play::views {

void drawConnectionFailure(NVGcontext* vg, float cx, float cy, const std::string& message)
{
    NVGcolor danger = app::views::active().danger;

    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, 14.0f);
    nvgFillColor(vg, danger);
    nvgFill(vg);

    nvgFontSize(vg, 22.0f);
    nvgFillColor(vg, app::views::active().text);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, cx, cy + 54.0f, message.c_str(), nullptr);
}

void drawConnectionRing(NVGcontext* vg, float cx, float cy, const std::string& label,
                        int stageIndex, int stageTotal)
{
    nvgSave(vg);

    NVGcolor accent = app::views::active().accent;

    const float radius = 54.0f;
    const float strokeW = 8.0f;
    const float ringCy = cy - 24.0f;

    NVGcolor track = app::views::active().textMuted;
    track.a = 0.22f;
    nvgBeginPath(vg);
    nvgCircle(vg, cx, ringCy, radius);
    nvgStrokeWidth(vg, strokeW);
    nvgStrokeColor(vg, track);
    nvgStroke(vg);

    float t = static_cast<float>(brls::getCPUTimeUsec() % 1000000) / 1000000.0f;
    float a0 = t * 2.0f * NVG_PI - NVG_PI * 0.5f;
    float a1 = a0 + NVG_PI * 0.55f;
    nvgLineCap(vg, NVG_ROUND);
    nvgBeginPath(vg);
    nvgArc(vg, cx, ringCy, radius, a0, a1, NVG_CW);
    nvgStrokeWidth(vg, strokeW);
    nvgStrokeColor(vg, accent);
    nvgStroke(vg);

    if (stageTotal > 0) {
        std::string count = std::to_string(stageIndex) + " / " + std::to_string(stageTotal);
        nvgFontSize(vg, 26.0f);
        nvgFillColor(vg, app::views::active().text);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, cx, ringCy, count.c_str(), nullptr);
    }

    nvgFontSize(vg, 20.0f);
    nvgFillColor(vg, app::views::active().textMuted);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, cx, ringCy + radius + 28.0f, label.c_str(), nullptr);

    nvgRestore(vg);
}

}
