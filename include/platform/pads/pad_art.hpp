#pragma once

#include <cstdint>

#include "platform/pads/pad.hpp"

namespace akira::platform {

class PadArt {
public:
    static auto path(const PadDescription& pad) -> const char*
    {
        if (pad.kind == PadPathKind::JoyCon)
            return pad.style == HidNpadStyleTag_NpadHandheld ? "img/pads/switch_handheld.png" : "img/pads/switch_joycons.png";
        if (pad.kind == PadPathKind::SwitchPro)
            return "img/pads/switch_pro.png";
        if (pad.vendor_id == sonyVendor)
        {
            switch (pad.product_id)
            {
                case 0x0ce6:
                case 0x0df2: return "img/pads/dualsense.png";
                case 0x09cc:
                case 0x05c4: return "img/pads/dualshock4.png";
                case 0x0268: return "img/pads/dualshock3.png";
                default: break;
            }
        }
        if (pad.vendor_id == nintendoVendor && pad.product_id == 0x2009)
            return "img/pads/switch_pro.png";
        return "img/pads/gamepad.png";
    }

    static auto twoBatteries(const PadDescription& pad) -> bool { return pad.kind == PadPathKind::JoyCon; }

private:
    static constexpr std::uint16_t sonyVendor = 0x054c;
    static constexpr std::uint16_t nintendoVendor = 0x057e;
};

}
