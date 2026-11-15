#ifndef AKIRA_BATTERY_PIPS_HPP
#define AKIRA_BATTERY_PIPS_HPP

#include <borealis.hpp>
#include <switch.h>

class BatteryPips : public brls::View {
public:
    BatteryPips(HidNpadIdType npad, bool split, float width, float height);

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

private:
    void drawOne(NVGcontext* vg, float x, float y, float w, float h,
                 const HidPowerInfo& info) const;

    HidNpadIdType m_npad;
    bool          m_split;
};

#endif // AKIRA_BATTERY_PIPS_HPP
