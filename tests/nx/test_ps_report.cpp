
#include "test_util.hpp"

#include "input/ps_report.hpp"

#include <cstring>

using namespace akira::input;

namespace {

constexpr std::size_t kLen = 78;

struct Report {
    std::uint8_t data[kLen] = {};

    Report()
    {
        std::memset(data, 0, sizeof(data));
        data[0] = 0x31;
        data[9] = 8;
        data[34] = 0x80;
        data[38] = 0x80;
    }

    void Press(int byte, std::uint8_t mask) { data[byte] |= mask; }

    void Touch(int slot, std::uint8_t id, std::uint16_t x, std::uint16_t y)
    {
        std::uint8_t* p = data + (slot == 0 ? 34 : 38);
        p[0] = (std::uint8_t)(id & 0x7f);
        p[1] = (std::uint8_t)(x & 0xff);
        p[2] = (std::uint8_t)(((x >> 8) & 0x0f) | ((y & 0x0f) << 4));
        p[3] = (std::uint8_t)(y >> 4);
    }
};

} // namespace

TEST(ps_report_rejects_a_short_report)
{
    Report r;
    PsPadState s;
    CHECK(!ParseDualSense31(r.data, 20, &s));
}

TEST(ps_report_rejects_another_report_id)
{
    Report r;
    r.data[0] = 0x01;
    PsPadState s;
    CHECK(!ParseDualSense31(r.data, kLen, &s));
}

TEST(ps_report_reads_trigger_pressure)
{
    Report r;
    r.data[6] = 137;
    r.data[7] = 42;

    PsPadState s;
    REQUIRE(ParseDualSense31(r.data, kLen, &s));
    CHECK_EQ((int)s.l2, 137);
    CHECK_EQ((int)s.r2, 42);
}

TEST(ps_report_keeps_create_options_and_mute_distinct)
{
    Report r;
    r.Press(10, 0x10);
    r.Press(10, 0x20);
    r.Press(11, 0x04);

    PsPadState s;
    REQUIRE(ParseDualSense31(r.data, kLen, &s));
    CHECK(s.buttons & PsButton_Create);
    CHECK(s.buttons & PsButton_Options);
    CHECK(s.buttons & PsButton_Mute);
    CHECK(!(s.buttons & PsButton_Touchpad));
}

TEST(ps_report_separates_a_touchpad_click_from_those_buttons)
{
    Report r;
    r.Press(11, 0x02);
    r.Touch(0, 3, 100, 500);

    PsPadState s;
    REQUIRE(ParseDualSense31(r.data, kLen, &s));
    CHECK(s.buttons & PsButton_Touchpad);
    CHECK(!(s.buttons & PsButton_Create));
    CHECK(!(s.buttons & PsButton_Options));
    CHECK(!(s.buttons & PsButton_Mute));
    CHECK(s.touch[0].down);
    CHECK_EQ((int)s.touch[0].x, 100);
}

TEST(ps_report_reads_both_touch_points)
{
    Report r;
    r.Touch(0, 1, 1919, 1079);
    r.Touch(1, 2, 5, 7);

    PsPadState s;
    REQUIRE(ParseDualSense31(r.data, kLen, &s));

    CHECK(s.touch[0].down);
    CHECK_EQ((int)s.touch[0].id, 1);
    CHECK_EQ((int)s.touch[0].x, 1919);
    CHECK_EQ((int)s.touch[0].y, 1079);

    CHECK(s.touch[1].down);
    CHECK_EQ((int)s.touch[1].id, 2);
    CHECK_EQ((int)s.touch[1].x, 5);
    CHECK_EQ((int)s.touch[1].y, 7);
}

TEST(ps_report_reports_an_empty_slot_as_up)
{
    Report r;
    r.Touch(0, 4, 300, 300);

    PsPadState s;
    REQUIRE(ParseDualSense31(r.data, kLen, &s));
    CHECK(s.touch[0].down);
    CHECK(!s.touch[1].down);
}

TEST(ps_report_maps_the_dpad_including_diagonals)
{
    struct Case { std::uint8_t raw; std::uint32_t expect; };
    const Case cases[] = {
        { 0, PsButton_Up },
        { 1, PsButton_Up | PsButton_Right },
        { 2, PsButton_Right },
        { 3, PsButton_Down | PsButton_Right },
        { 4, PsButton_Down },
        { 5, PsButton_Down | PsButton_Left },
        { 6, PsButton_Left },
        { 7, PsButton_Up | PsButton_Left },
        { 8, 0 },
    };

    for (const Case& c : cases) {
        Report r;
        r.data[9] = c.raw;

        PsPadState s;
        REQUIRE(ParseDualSense31(r.data, kLen, &s));

        const std::uint32_t dpad = s.buttons &
            (PsButton_Up | PsButton_Down | PsButton_Left | PsButton_Right);
        CHECK_EQ(dpad, c.expect);
    }
}

TEST(ps_report_reads_the_face_and_shoulder_buttons)
{
    Report r;
    r.Press(9, 0x20);
    r.Press(9, 0x40);
    r.Press(10, 0x01);
    r.Press(10, 0x80);

    PsPadState s;
    REQUIRE(ParseDualSense31(r.data, kLen, &s));
    CHECK(s.buttons & PsButton_Cross);
    CHECK(s.buttons & PsButton_Circle);
    CHECK(s.buttons & PsButton_L1);
    CHECK(s.buttons & PsButton_R3);
    CHECK(!(s.buttons & PsButton_Square));
    CHECK(!(s.buttons & PsButton_Triangle));
}

TEST(ps_model_lookup_distinguishes_dualsense_from_dualshock4)
{
    const PsModel* ds = FindPsModel(0x054c, 0x0ce6);
    REQUIRE(ds != nullptr);
    CHECK_EQ((int)ds->report_id, 0x31);
    CHECK_EQ((int)ds->surface.height, 1080);

    const PsModel* ds4 = FindPsModel(0x054c, 0x09cc);
    REQUIRE(ds4 != nullptr);
    CHECK_EQ((int)ds4->report_id, 0x11);
    CHECK_EQ((int)ds4->surface.height, 942);

    CHECK(FindPsModel(0x054c, 0xffff) == nullptr);
}

namespace {

struct Ds4Report {
    std::uint8_t data[78] = {};

    Ds4Report()
    {
        std::memset(data, 0, sizeof(data));
        data[0] = 0x11;
        data[7] = 8;
        data[35] = 1;
        data[37] = 0x80;
        data[41] = 0x80;
    }

    void Touch(int slot, std::uint8_t id, std::uint16_t x, std::uint16_t y)
    {
        std::uint8_t* p = data + (slot == 0 ? 37 : 41);
        p[0] = (std::uint8_t)(id & 0x7f);
        p[1] = (std::uint8_t)(x & 0xff);
        p[2] = (std::uint8_t)(((x >> 8) & 0x0f) | ((y & 0x0f) << 4));
        p[3] = (std::uint8_t)(y >> 4);
    }
};

} // namespace

TEST(ds4_report_reads_triggers_and_buttons_at_its_own_offsets)
{
    Ds4Report r;
    r.data[10] = 200;
    r.data[11] = 15;
    r.data[8] |= 0x10;
    r.data[8] |= 0x20;

    PsPadState s;
    REQUIRE(ParseDualShock4_11(r.data, sizeof(r.data), &s));
    CHECK_EQ((int)s.l2, 200);
    CHECK_EQ((int)s.r2, 15);
    CHECK(s.buttons & PsButton_Create);
    CHECK(s.buttons & PsButton_Options);
}

TEST(ds4_report_never_reports_a_mute_button)
{
    Ds4Report r;
    r.data[9] = 0xfc;

    PsPadState s;
    REQUIRE(ParseDualShock4_11(r.data, sizeof(r.data), &s));
    CHECK(!(s.buttons & PsButton_Mute));
    CHECK(!(s.buttons & PsButton_Ps));
    CHECK(!(s.buttons & PsButton_Touchpad));
}

TEST(ds4_report_ignores_touch_when_no_reports_are_live)
{
    Ds4Report r;
    r.Touch(0, 1, 400, 400);
    r.data[35] = 0;

    PsPadState s;
    REQUIRE(ParseDualShock4_11(r.data, sizeof(r.data), &s));
    CHECK(!s.touch_fresh);
    CHECK(!s.touch[0].down);
}

TEST(ds4_report_reads_touch_when_a_report_is_live)
{
    Ds4Report r;
    r.Touch(0, 9, 1000, 900);

    PsPadState s;
    REQUIRE(ParseDualShock4_11(r.data, sizeof(r.data), &s));
    CHECK(s.touch_fresh);
    CHECK(s.touch[0].down);
    CHECK_EQ((int)s.touch[0].x, 1000);
    CHECK_EQ((int)s.touch[0].y, 900);
}

TEST(ps_report_dispatches_on_the_model_not_the_bytes)
{
    Report ds;
    ds.data[6] = 99;

    const PsModel* ds4 = FindPsModel(0x054c, 0x09cc);
    REQUIRE(ds4 != nullptr);

    PsPadState s;
    CHECK(!ParsePsReport(*ds4, ds.data, kLen, &s));

    const PsModel* dualsense = FindPsModel(0x054c, 0x0ce6);
    REQUIRE(dualsense != nullptr);
    REQUIRE(ParsePsReport(*dualsense, ds.data, kLen, &s));
    CHECK_EQ((int)s.l2, 99);
}
