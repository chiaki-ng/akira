#ifndef AKIRA_INPUT_PAD_NAMES_HPP
#define AKIRA_INPUT_PAD_NAMES_HPP

#include <cstdint>

namespace akira::platform::extended {

inline const char* FindPadName(std::uint16_t vendor_id, std::uint16_t product_id)
{
    struct Entry { std::uint16_t vid, pid; const char* name; };

    static const Entry kNames[] = {
        { 0x054c, 0x0268, "DualShock 3" },
        { 0x0f0d, 0x00f6, "Hori ONYX" },
        { 0x1532, 0x1009, "Razer Raiju Ultimate" },
        { 0x1532, 0x100a, "Razer Raiju Tournament" },
        { 0x2e95, 0x7725, "SCUF Vantage 2" },

        { 0x045e, 0x02e0, "Xbox One S Controller" },
        { 0x045e, 0x02fd, "Xbox One S Controller" },
        { 0x045e, 0x0b00, "Xbox Elite 2 Controller" },
        { 0x045e, 0x0b05, "Xbox Elite 2 Controller" },
        { 0x045e, 0x0b0a, "Xbox Adaptive Controller" },

        { 0x057e, 0x2006, "Joy-Con (L)" },
        { 0x057e, 0x2007, "Joy-Con (R)" },
        { 0x057e, 0x2009, "Pro Controller" },
        { 0x057e, 0x2017, "SNES Controller" },
        { 0x057e, 0x2019, "N64 Controller" },
        { 0x057e, 0x201a, "Mega Drive Controller" },
        { 0x057e, 0x0306, "Wii Remote" },
        { 0x057e, 0x0330, "Wii U Pro Controller" },

        { 0x05a0, 0x3232, "8BitDo Zero" },
        { 0x2dc8, 0x2100, "8BitDo SN30 Pro" },
        { 0x2dc8, 0x2101, "8BitDo SN30 Pro" },
        { 0x2dc8, 0x3012, "8BitDo Ultimate" },

        { 0x3537, 0x1022, "GameSir G7 Pro" },

        { 0x3250, 0x1002, "Atari VCS Controller" },
        { 0x1d6b, 0x0246, "AtGames Legends Pinball" },
        { 0x20bc, 0x5501, "Betop 2585N2" },
        { 0x2e2c, 0x0002, "Bionik Vulkan" },
        { 0x0f0d, 0x1011, "GameStick Controller" },
        { 0x1d79, 0x0009, "Gembox Controller" },
        { 0x2e24, 0x200a, "Hyperkin Scout" },
        { 0x15e4, 0x0132, "ION iCade" },
        { 0x1949, 0x0403, "iPega Controller" },
        { 0x0079, 0x181c, "LanShen X1Pro" },
        { 0x0738, 0x5266, "Mad Catz C.T.R.L.R" },
        { 0x0738, 0x5250, "Mad Catz C.T.R.L.R" },
        { 0x0738, 0x5269, "Mad Catz L.Y.N.X. 3" },
        { 0x0955, 0x7214, "NVIDIA Shield Controller" },
        { 0x2836, 0x0001, "OUYA Controller" },
        { 0x20d6, 0x89e5, "Moga Hero" },
        { 0x20d6, 0x0dad, "Moga Pro" },
        { 0x20d6, 0x6271, "Moga Pro 2" },
        { 0x1532, 0x0900, "Razer Serval" },
        { 0x1038, 0x1412, "SteelSeries Free" },
        { 0x0111, 0x1420, "SteelSeries Nimbus" },
        { 0x0111, 0x1431, "SteelSeries Stratus Duo" },
        { 0x0111, 0x1419, "SteelSeries Stratus XL" },
        { 0x2717, 0x3144, "Xiaomi Mi Controller" },
    };

    for (const Entry& e : kNames) {
        if (e.vid == vendor_id && e.pid == product_id)
            return e.name;
    }
    return nullptr;
}

inline bool PadReportsBattery(std::uint16_t vendor_id, std::uint16_t product_id)
{
    struct Entry { std::uint16_t vid, pid; };

    static const Entry kReal[] = {
        { 0x054c, 0x0ce6 }, { 0x054c, 0x0df2 },
        { 0x054c, 0x0268 },
        { 0x054c, 0x05c4 }, { 0x054c, 0x09cc },
        { 0x0f0d, 0x00f6 }, { 0x1532, 0x1009 },
        { 0x1532, 0x100a }, { 0x2e95, 0x7725 },
        { 0x045e, 0x02e0 }, { 0x045e, 0x02fd }, { 0x045e, 0x0b00 },
        { 0x045e, 0x0b05 }, { 0x045e, 0x0b0a },
        { 0x05a0, 0x3232 }, { 0x2dc8, 0x2100 },
        { 0x2dc8, 0x2101 }, { 0x2dc8, 0x3012 },
        { 0x1949, 0x0402 },
        { 0x2836, 0x0001 },
        { 0x2717, 0x3144 },
        { 0x20d6, 0x89e5 }, { 0x20d6, 0x0dad }, { 0x20d6, 0x6271 },
        { 0x057e, 0x0306 }, { 0x057e, 0x0330 },
    };

    for (const Entry& e : kReal) {
        if (e.vid == vendor_id && e.pid == product_id)
            return true;
    }
    return false;
}

} // namespace akira::platform::extended

#endif // AKIRA_INPUT_PAD_NAMES_HPP
