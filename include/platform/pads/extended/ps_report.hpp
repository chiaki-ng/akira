#ifndef AKIRA_INPUT_PS_REPORT_HPP
#define AKIRA_INPUT_PS_REPORT_HPP

#include <cstddef>
#include <cstdint>

namespace akira::platform::extended {


enum PsButton : uint32_t {
    PsButton_Cross    = 1u << 0,
    PsButton_Circle   = 1u << 1,
    PsButton_Square   = 1u << 2,
    PsButton_Triangle = 1u << 3,
    PsButton_L1       = 1u << 4,
    PsButton_R1       = 1u << 5,
    PsButton_L2       = 1u << 6,
    PsButton_R2       = 1u << 7,
    PsButton_L3       = 1u << 8,
    PsButton_R3       = 1u << 9,
    PsButton_Create   = 1u << 10,
    PsButton_Options  = 1u << 11,
    PsButton_Ps       = 1u << 12,
    PsButton_Touchpad = 1u << 13,
    PsButton_Mute     = 1u << 14,
    PsButton_Up       = 1u << 15,
    PsButton_Down     = 1u << 16,
    PsButton_Left     = 1u << 17,
    PsButton_Right    = 1u << 18,
};

struct PsTouchPoint {
    bool     down = false;
    uint8_t  id   = 0;
    uint16_t x    = 0;
    uint16_t y    = 0;
};

struct PsPadState {
    bool         valid = false;
    uint32_t     buttons = 0;
    uint8_t      l2 = 0;
    uint8_t      r2 = 0;
    PsTouchPoint touch[2];

    bool         touch_fresh = false;
};

struct PsTouchSurface {
    uint16_t width  = 0;
    uint16_t height = 0;
};

struct PsModel {
    uint16_t       vendor_id  = 0;
    uint16_t       product_id = 0;
    uint8_t        report_id  = 0;
    bool           has_touchpad = false;
    PsTouchSurface surface;
    const char*    name = "";
};

namespace detail {

inline PsTouchPoint ParseTouchPoint(const uint8_t* p)
{
    PsTouchPoint out;
    out.down = (p[0] & 0x80) == 0;
    out.id   = (uint8_t)(p[0] & 0x7f);
    out.x    = (uint16_t)(p[1] | ((p[2] & 0x0f) << 8));
    out.y    = (uint16_t)((p[2] >> 4) | (p[3] << 4));
    return out;
}

inline uint32_t ParseDpad(uint8_t dpad)
{
    switch (dpad) {
        case 0: return PsButton_Up;
        case 1: return PsButton_Up | PsButton_Right;
        case 2: return PsButton_Right;
        case 3: return PsButton_Down | PsButton_Right;
        case 4: return PsButton_Down;
        case 5: return PsButton_Down | PsButton_Left;
        case 6: return PsButton_Left;
        case 7: return PsButton_Up | PsButton_Left;
        default: return 0;
    }
}

} // namespace detail

inline bool ParseDualSense31(const uint8_t* data, size_t len, PsPadState* out)
{
    if (data == nullptr || out == nullptr || len < 42)
        return false;
    if (data[0] != 0x31)
        return false;

    PsPadState s;
    s.valid = true;

    s.l2 = data[6];
    s.r2 = data[7];

    const uint8_t b0 = data[9];
    const uint8_t b1 = data[10];
    const uint8_t b2 = data[11];

    s.buttons |= detail::ParseDpad((uint8_t)(b0 & 0x0f));

    if (b0 & 0x10) s.buttons |= PsButton_Square;
    if (b0 & 0x20) s.buttons |= PsButton_Cross;
    if (b0 & 0x40) s.buttons |= PsButton_Circle;
    if (b0 & 0x80) s.buttons |= PsButton_Triangle;

    if (b1 & 0x01) s.buttons |= PsButton_L1;
    if (b1 & 0x02) s.buttons |= PsButton_R1;
    if (b1 & 0x04) s.buttons |= PsButton_L2;
    if (b1 & 0x08) s.buttons |= PsButton_R2;
    if (b1 & 0x10) s.buttons |= PsButton_Create;
    if (b1 & 0x20) s.buttons |= PsButton_Options;
    if (b1 & 0x40) s.buttons |= PsButton_L3;
    if (b1 & 0x80) s.buttons |= PsButton_R3;

    if (b2 & 0x01) s.buttons |= PsButton_Ps;
    if (b2 & 0x02) s.buttons |= PsButton_Touchpad;
    if (b2 & 0x04) s.buttons |= PsButton_Mute;

    s.touch[0] = detail::ParseTouchPoint(data + 34);
    s.touch[1] = detail::ParseTouchPoint(data + 38);
    s.touch_fresh = true;

    *out = s;
    return true;
}

inline bool ParseDualShock4_11(const uint8_t* data, size_t len, PsPadState* out)
{
    if (data == nullptr || out == nullptr || len < 45)
        return false;
    if (data[0] != 0x11)
        return false;

    PsPadState s;
    s.valid = true;

    s.l2 = data[10];
    s.r2 = data[11];

    const uint8_t b0 = data[7];
    const uint8_t b1 = data[8];
    const uint8_t b2 = data[9];

    s.buttons |= detail::ParseDpad((uint8_t)(b0 & 0x0f));

    if (b0 & 0x10) s.buttons |= PsButton_Square;
    if (b0 & 0x20) s.buttons |= PsButton_Cross;
    if (b0 & 0x40) s.buttons |= PsButton_Circle;
    if (b0 & 0x80) s.buttons |= PsButton_Triangle;

    if (b1 & 0x01) s.buttons |= PsButton_L1;
    if (b1 & 0x02) s.buttons |= PsButton_R1;
    if (b1 & 0x04) s.buttons |= PsButton_L2;
    if (b1 & 0x08) s.buttons |= PsButton_R2;
    if (b1 & 0x10) s.buttons |= PsButton_Create;
    if (b1 & 0x20) s.buttons |= PsButton_Options;
    if (b1 & 0x40) s.buttons |= PsButton_L3;
    if (b1 & 0x80) s.buttons |= PsButton_R3;

    if (b2 & 0x01) s.buttons |= PsButton_Ps;
    if (b2 & 0x02) s.buttons |= PsButton_Touchpad;

    if (data[35] > 0) {
        s.touch[0] = detail::ParseTouchPoint(data + 37);
        s.touch[1] = detail::ParseTouchPoint(data + 41);
        s.touch_fresh = true;
    }

    *out = s;
    return true;
}

inline const PsModel* FindPsModel(uint16_t vendor_id, uint16_t product_id)
{
    static const PsModel kModels[] = {
        { 0x054c, 0x0ce6, 0x31, true, { 1920, 1080 }, "DualSense" },
        { 0x054c, 0x0df2, 0x31, true, { 1920, 1080 }, "DualSense Edge" },
        { 0x054c, 0x09cc, 0x11, true, { 1920,  942 }, "DualShock 4" },
        { 0x054c, 0x05c4, 0x11, true, { 1920,  942 }, "DualShock 4 v1" },
    };

    for (const PsModel& m : kModels) {
        if (m.vendor_id == vendor_id && m.product_id == product_id)
            return &m;
    }
    return nullptr;
}

inline bool ParsePsReport(const PsModel& model, const uint8_t* data, size_t len, PsPadState* out)
{
    if (model.report_id == 0x31)
        return ParseDualSense31(data, len, out);
    if (model.report_id == 0x11)
        return ParseDualShock4_11(data, len, out);
    return false;
}

} // namespace akira::platform::extended

#endif // AKIRA_INPUT_PS_REPORT_HPP
