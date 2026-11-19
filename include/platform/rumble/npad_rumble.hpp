#pragma once

#include <array>
#include <cstdint>
#include <mutex>

#include <switch.h>

#include "platform/pads/rumble.hpp"

namespace akira::platform {

class NpadRumble final : public PadRumble {
public:
    NpadRumble();

    void playerOne(Wave wave) override;
    void npad(unsigned npad, Wave wave) override;
    void refresh(unsigned npad) override;

private:
    struct Device {
        std::array<HidVibrationDeviceHandle, 2> handles{};
        s32 count = 0;
        u32 style = 0;
    };

    static constexpr unsigned players = 8;

    static void send(Device& device, Wave wave);
    static void prepare(Device& device, HidNpadIdType id, u32 styles);
    auto deviceFor(unsigned npad) -> Device&;

    std::mutex mutex;
    Device handheld;
    std::array<Device, players> devices{};
};

}
