#include "platform/pad_settings/extended/lightbar_picker.hpp"

#include <borealis/core/i18n.hpp>

#include <algorithm>
#include <cmath>

using namespace brls::literals;

namespace {

constexpr float kFieldHeight = 150.0f;
constexpr float kStripHeight = 26.0f;
constexpr float kGap         = 12.0f;

void HsvToRgb(float h, float s, float v,
              std::uint8_t* r, std::uint8_t* g, std::uint8_t* b)
{
    const float c = v * s;
    const float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    const float m = v - c;

    float rf = 0.0f, gf = 0.0f, bf = 0.0f;
    if      (h <  60.0f) { rf = c; gf = x; }
    else if (h < 120.0f) { rf = x; gf = c; }
    else if (h < 180.0f) { gf = c; bf = x; }
    else if (h < 240.0f) { gf = x; bf = c; }
    else if (h < 300.0f) { rf = x; bf = c; }
    else                 { rf = c; bf = x; }

    *r = (std::uint8_t)std::lround((rf + m) * 255.0f);
    *g = (std::uint8_t)std::lround((gf + m) * 255.0f);
    *b = (std::uint8_t)std::lround((bf + m) * 255.0f);
}

void RgbToHsv(std::uint8_t r, std::uint8_t g, std::uint8_t b,
              float* h, float* s, float* v)
{
    const float rf = r / 255.0f, gf = g / 255.0f, bf = b / 255.0f;
    const float mx = std::max({rf, gf, bf});
    const float mn = std::min({rf, gf, bf});
    const float d  = mx - mn;

    *v = mx;
    *s = mx <= 0.0f ? 0.0f : d / mx;

    if (d <= 0.0f)            *h = 0.0f;
    else if (mx == rf)        *h = 60.0f * std::fmod((gf - bf) / d, 6.0f);
    else if (mx == gf)        *h = 60.0f * (((bf - rf) / d) + 2.0f);
    else                      *h = 60.0f * (((rf - gf) / d) + 4.0f);

    if (*h < 0.0f) *h += 360.0f;
}

} // namespace

LightbarPicker::LightbarPicker(std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    RgbToHsv(r, g, b, &m_h, &m_s, &m_v);

    this->setHeight(kFieldHeight + kStripHeight + kGap);
    this->setFocusable(true);

    this->registerAction("akira/settings/lightbar_switch"_i18n,
                         brls::ControllerButton::BUTTON_Y,
                         [this](brls::View*) { m_on_hue = !m_on_hue; return true; }, false);

    const auto step = [this](float sx, float sv, float hue) {
        return [this, sx, sv, hue](brls::View*) {
            if (m_on_hue) moveHue(hue);
            else          moveField(sx, sv);
            return true;
        };
    };

    this->registerAction("", brls::ControllerButton::BUTTON_NAV_LEFT,  step(-0.02f, 0.0f, -4.0f), true);
    this->registerAction("", brls::ControllerButton::BUTTON_NAV_RIGHT, step( 0.02f, 0.0f,  4.0f), true);
    this->registerAction("", brls::ControllerButton::BUTTON_NAV_UP,    step(0.0f,  0.02f, 0.0f), true);
    this->registerAction("", brls::ControllerButton::BUTTON_NAV_DOWN,  step(0.0f, -0.02f, 0.0f), true);
}

void LightbarPicker::moveHue(float delta)
{
    m_h = std::fmod(m_h + delta + 360.0f, 360.0f);
    emit();
}

void LightbarPicker::moveField(float ds, float dv)
{
    m_s = std::clamp(m_s + ds, 0.0f, 1.0f);
    m_v = std::clamp(m_v + dv, 0.0f, 1.0f);
    emit();
}

void LightbarPicker::emit()
{
    if (!m_changed)
        return;

    std::uint8_t r = 0, g = 0, b = 0;
    HsvToRgb(m_h, m_s, m_v, &r, &g, &b);
    m_changed(r, g, b);
}

void LightbarPicker::draw(NVGcontext* vg, float x, float y, float width, float height,
                          brls::Style style, brls::FrameContext* ctx)
{
    const float fieldY = y;
    const float stripY = y + kFieldHeight + kGap;

    std::uint8_t hr = 0, hg = 0, hb = 0;
    HsvToRgb(m_h, 1.0f, 1.0f, &hr, &hg, &hb);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, fieldY, width, kFieldHeight, 6.0f);
    NVGpaint sat = nvgLinearGradient(vg, x, fieldY, x + width, fieldY,
                                     nvgRGB(255, 255, 255), nvgRGB(hr, hg, hb));
    nvgFillPaint(vg, sat);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, fieldY, width, kFieldHeight, 6.0f);
    NVGpaint val = nvgLinearGradient(vg, x, fieldY, x, fieldY + kFieldHeight,
                                     nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 255));
    nvgFillPaint(vg, val);
    nvgFill(vg);

    static const std::uint8_t stops[7][3] = {
        {255, 0, 0}, {255, 255, 0}, {0, 255, 0},
        {0, 255, 255}, {0, 0, 255}, {255, 0, 255}, {255, 0, 0},
    };
    const float seg = width / 6.0f;
    for (int i = 0; i < 6; i++) {
        const float sx = x + seg * (float)i;
        nvgBeginPath(vg);
        nvgRect(vg, sx, stripY, seg + 1.0f, kStripHeight);
        NVGpaint p = nvgLinearGradient(vg, sx, stripY, sx + seg, stripY,
                                       nvgRGB(stops[i][0], stops[i][1], stops[i][2]),
                                       nvgRGB(stops[i+1][0], stops[i+1][1], stops[i+1][2]));
        nvgFillPaint(vg, p);
        nvgFill(vg);
    }

    std::uint8_t cr = 0, cg = 0, cb = 0;
    HsvToRgb(m_h, m_s, m_v, &cr, &cg, &cb);

    const auto ring = [&](float cx, float cy, float rad, bool active) {
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, rad + 1.0f);
        nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 160));
        nvgStrokeWidth(vg, 2.0f);
        nvgStroke(vg);

        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, rad);
        nvgStrokeColor(vg, nvgRGBA(255, 255, 255, active ? 255 : 140));
        nvgStrokeWidth(vg, active ? 3.0f : 2.0f);
        nvgStroke(vg);
    };

    ring(x + m_s * width, fieldY + (1.0f - m_v) * kFieldHeight, 7.0f, !m_on_hue);
    ring(x + (m_h / 360.0f) * width, stripY + kStripHeight / 2.0f, 8.0f, m_on_hue);

    Box::draw(vg, x, y, width, height, style, ctx);
}
