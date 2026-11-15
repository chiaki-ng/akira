#include "platform/stream/couch/battery_pips.hpp"

namespace akira::platform::couch
{

BatteryPips::BatteryPips(HidNpadIdType npad, bool split, float width, float height)
    : m_npad(npad)
    , m_split(split)
{
    this->setWidth(width);
    this->setHeight(height);
}

void BatteryPips::draw(NVGcontext* vg, float x, float y, float width, float height,
                       brls::Style style, brls::FrameContext* ctx)
{
    (void)style; (void)ctx;

    if (m_split) {
        HidPowerInfo left{}, right{};
        hidGetNpadPowerInfoSplit(m_npad, &left, &right);

        const float cellW = width * 0.44f;
        drawOne(vg, x, y, cellW, height, left);
        drawOne(vg, x + width - cellW, y, cellW, height, right);
        return;
    }

    HidPowerInfo info{};
    hidGetNpadPowerInfoSingle(m_npad, &info);
    drawOne(vg, x + (width - width * 0.44f) * 0.5f, y, width * 0.44f, height, info);
}

void BatteryPips::drawOne(NVGcontext* vg, float x, float y, float w, float h,
                          const HidPowerInfo& info) const
{
    const NVGcolor ink  = nvgRGB(140, 155, 172);
    const NVGcolor fill = info.is_charging ? nvgRGB(64, 208, 122) : nvgRGB(232, 238, 245);

    const float bodyW = w * 0.84f;
    const float bodyH = h * 0.58f;
    const float by    = y + (h - bodyH) * 0.5f;

    nvgStrokeColor(vg, ink);
    nvgStrokeWidth(vg, 1.5f);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, by, bodyW, bodyH, 2.5f);
    nvgStroke(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + bodyW + 1.0f, by + bodyH * 0.30f, w * 0.10f, bodyH * 0.40f, 1.0f);
    nvgFillColor(vg, ink);
    nvgFill(vg);

    const float level = info.battery_level > 4 ? 1.0f : (float)info.battery_level / 4.0f;
    if (level > 0.0f) {
        const float pad = 2.0f;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x + pad, by + pad,
                       (bodyW - 2.0f * pad) * level, bodyH - 2.0f * pad, 1.5f);
        nvgFillColor(vg, fill);
        nvgFill(vg);
    }
}

}

