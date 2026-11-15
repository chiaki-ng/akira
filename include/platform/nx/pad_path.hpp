#ifndef AKIRA_INPUT_PAD_PATH_HPP
#define AKIRA_INPUT_PAD_PATH_HPP

#include <switch.h>

#include <libchiaking/play_types.hpp>

#include <cstdint>
#include <memory>
#include <vector>

#include "input/pad.hpp"
#include "platform/nx/pad_output_state.hpp"

class ExtendedInputManager;

namespace akira::input {

class PadPath : public Pad {
public:
    virtual PadPathKind kind() const = 0;

    virtual const char* label() const = 0;

    virtual HidNpadIdType   npad()  const = 0;
    virtual HidNpadStyleTag style() const = 0;

    virtual PadCapabilities capabilities() const = 0;

    virtual bool switchNative() const { return true; }
    virtual uint16_t vendorId() const { return 0; }
    virtual uint16_t productId() const { return 0; }

    virtual const uint8_t* address() const { return nullptr; }

    struct TriggerEffect {
        uint8_t type = 0;
        uint8_t params[10] = {};
    };

    virtual void sendTriggerEffects(const TriggerEffect&, const TriggerEffect&) {}

    virtual void sendEffectIntensity(uint8_t, uint8_t) {}

    virtual void sendLightbar(uint8_t, uint8_t, uint8_t) {}

    virtual void setExcludedNpads(uint64_t) {}
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
