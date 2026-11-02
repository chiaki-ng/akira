#ifndef AKIRA_INPUT_PAD_PATH_HPP
#define AKIRA_INPUT_PAD_PATH_HPP

#include <switch.h>

#include <libchiaking/play_types.hpp>

#include <cstdint>
#include <memory>
#include <vector>

#include "input/pad_output_state.hpp"

class ExtendedInputManager;

namespace akira::input {

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

class PadPath {
public:
    virtual ~PadPath() = default;

    virtual PadPathKind kind() const = 0;

    virtual const char* label() const = 0;

    virtual HidNpadIdType   npad()  const = 0;
    virtual HidNpadStyleTag style() const = 0;

    virtual PadCapabilities capabilities() const = 0;

    virtual bool poll() = 0;

    virtual void readButtons(libchiaking::controllers::ControllerState& state) { (void)state; }

    virtual HidAnalogStickState stickPos(int index) const = 0;

    virtual void readTriggers(libchiaking::controllers::ControllerState& state) = 0;

    virtual bool readGyro(HidSixAxisSensorState* out) = 0;

    virtual void resetMotion() = 0;

    virtual bool readTouchpad(libchiaking::controllers::ControllerInput& input) { (void)input; return false; }

    virtual void sendRumble(float left, float right, float freqLow, float freqHigh) = 0;

    virtual bool nativeRumble() const { return false; }

    virtual bool switchNative() const { return true; }
    virtual uint16_t vendorId() const { return 0; }
    virtual uint16_t productId() const { return 0; }

    virtual const uint8_t* address() const { return nullptr; }

    struct TriggerEffect {
        uint8_t type = 0;
        uint8_t params[10] = {};
    };

    virtual void sendTriggerEffects(const TriggerEffect& left, const TriggerEffect& right)
    {
        (void)left;
        (void)right;
    }

    virtual void sendEffectIntensity(uint8_t vibration, uint8_t trigger)
    {
        (void)vibration;
        (void)trigger;
    }

    virtual void sendLightbar(uint8_t red, uint8_t green, uint8_t blue)
    {
        (void)red;
        (void)green;
        (void)blue;
    }

    virtual uint64_t heldButtons() const = 0;

    virtual bool menuHeld() const { return (heldButtons() & HidNpadButton_Minus) != 0; }

    virtual bool usesButtonMapping() const { return true; }

    virtual void setExcludedNpads(uint64_t mask) { (void)mask; }
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


PadDriverState ResolvePadDriverState(const PadDescription& desc,
                                     ExtendedInputManager& extended);

PadDriver ResolvePadDriver(const PadDescription& desc, ExtendedInputManager& extended);

std::vector<PadDescription> DescribePads(ExtendedInputManager& extended);

std::vector<PadDescription> DescribePads();

std::unique_ptr<PadPath> MakePath(const PadDescription& desc, ExtendedInputManager& extended);

std::unique_ptr<PadPath> DefaultPadPath(ExtendedInputManager& extended);

} // namespace akira::input

#endif // AKIRA_INPUT_PAD_PATH_HPP
