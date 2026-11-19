#ifndef AKIRA_INPUT_PAD_ART_HPP
#define AKIRA_INPUT_PAD_ART_HPP

#include "platform/pads/pad.hpp"

namespace akira::platform::couch {

inline const char* PadArtPath(const platform::PadDescription& pad)
{
    if (pad.kind == platform::PadPathKind::JoyCon) {
        return pad.style == HidNpadStyleTag_NpadHandheld ? "img/pads/switch_handheld.png"
                                                         : "img/pads/switch_joycons.png";
    }

    if (pad.kind == platform::PadPathKind::SwitchPro)
        return "img/pads/switch_pro.png";

    if (pad.vendor_id == 0x054c) {
        switch (pad.product_id) {
            case 0x0ce6: case 0x0df2: return "img/pads/dualsense.png";
            case 0x09cc: case 0x05c4: return "img/pads/dualshock4.png";
            case 0x0268:              return "img/pads/dualshock3.png";
            default: break;
        }
    }
    if (pad.vendor_id == 0x057e && pad.product_id == 0x2009)
        return "img/pads/switch_pro.png";

    return "img/pads/gamepad.png";
}

inline bool PadHasTwoBatteries(const platform::PadDescription& pad)
{
    return pad.kind == platform::PadPathKind::JoyCon;
}

} // namespace akira::platform::couch

#endif // AKIRA_INPUT_PAD_ART_HPP
