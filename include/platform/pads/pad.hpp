#pragma once

#include <switch.h>

#include <cstdint>
#include <optional>

#include <libchiaking/play_types.hpp>

namespace akira::platform {

enum class PadPathKind {
    JoyCon,
    SwitchPro,
    McPsNative,
    McGeneric,
};

struct PadCapabilities {
    bool analog_triggers = false;
    bool touchpad        = false;
    bool gyro            = false;
    bool rumble          = false;
    bool battery         = false;
};

struct PadDescription {
    HidNpadIdType   npad  = HidNpadIdType_No1;
    PadPathKind     kind  = PadPathKind::JoyCon;
    HidNpadStyleTag style = HidNpadStyleTag_NpadFullKey;
    const char*     label = "";
    PadCapabilities caps;

    uint16_t vendor_id  = 0;
    uint16_t product_id = 0;

    uint8_t  bt_addr[6]{};
    bool     has_address = false;
};

class Pad {
public:
    virtual ~Pad() = default;

    virtual bool poll() = 0;

    virtual uint64_t heldButtons() const = 0;
    virtual bool menuHeld() const { return (heldButtons() & HidNpadButton_Minus) != 0; }
    virtual bool usesButtonMapping() const { return true; }

    virtual void readButtons(libchiaking::controllers::ControllerState&) {}
    virtual void readTriggers(libchiaking::controllers::ControllerState& state) = 0;
    virtual HidAnalogStickState stickPos(int index) const = 0;
    virtual bool readTouchpad(libchiaking::controllers::ControllerInput&) { return false; }

    virtual std::optional<HidSixAxisSensorState> readGyro() = 0;
    virtual void resetMotion() = 0;

    virtual bool nativeRumble() const { return false; }
    virtual void sendRumble(float left, float right, float freqLow, float freqHigh) = 0;

protected:
    static std::optional<HidSixAxisSensorState> readSixAxis(HidSixAxisSensorHandle handle)
    {
        HidSixAxisSensorState state{};
        if (hidGetSixAxisSensorStates(handle, &state, 1) == 0)
            return std::nullopt;
        return state;
    }
};

}
