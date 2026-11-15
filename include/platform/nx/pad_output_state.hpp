#ifndef AKIRA_INPUT_PAD_OUTPUT_STATE_HPP
#define AKIRA_INPUT_PAD_OUTPUT_STATE_HPP

#include <cstdint>

namespace akira::input {


enum class PadDriver {
    MissionControl = 0,

    Akira,
};

enum class PadDriverReason {
    Native = 0,
    Released,
    Unsupported,
    Basic,
    NotSupportedPad,
    NoAddress,
    NotWanted,
};

struct PadOutputInputs {
    bool supported_pad = false;

    bool profile_native = false;

    bool backend_enabled = true;

    bool ownership_refused = false;

    bool owns_output = false;

    bool wanted = false;

    bool address_valid = false;

    bool haptics_wanted = false;

    bool haptics_landing = true;
};

struct PadDriverState {
    PadDriver       driver = PadDriver::MissionControl;
    PadDriverReason reason = PadDriverReason::NotSupportedPad;
};

struct PadOutputState {
    PadDriver       driver = PadDriver::MissionControl;
    PadDriverReason reason = PadDriverReason::NotSupportedPad;

    bool want_claim = false;

    bool write_state = false;

    bool stream_haptics = false;
};

PadOutputState ResolvePadOutput(const PadOutputInputs& in);

const char* PadDriverName(PadDriver driver);
const char* PadDriverReasonName(PadDriverReason reason);

} // namespace akira::input

#endif // AKIRA_INPUT_PAD_OUTPUT_STATE_HPP
