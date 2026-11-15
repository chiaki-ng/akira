#include "platform/pads/extended/pad_output_state.hpp"

namespace akira::platform::extended {

const char* PadDriverName(PadDriver driver)
{
    return driver == PadDriver::Akira ? "akira" : "MissionControl";
}

const char* PadDriverReasonName(PadDriverReason reason)
{
    switch (reason) {
        case PadDriverReason::Native:          return "driving it";
        case PadDriverReason::Released:        return "native output is off";
        case PadDriverReason::Unsupported:     return "no output ownership";
        case PadDriverReason::Basic:           return "set to basic";
        case PadDriverReason::NoAddress:       return "address unknown";
        case PadDriverReason::NotWanted:       return "nothing streaming";
        case PadDriverReason::NotSupportedPad:
        default:                               return "not a pad we can drive";
    }
}

PadOutputState ResolvePadOutput(const PadOutputInputs& in)
{
    PadOutputState out;

    if (!in.supported_pad) {
        out.reason = PadDriverReason::NotSupportedPad;
        return out;
    }

    if (!in.backend_enabled) {
        out.reason = PadDriverReason::Released;
        return out;
    }

    if (!in.profile_native) {
        out.reason = PadDriverReason::Basic;
        return out;
    }

    if (in.ownership_refused) {
        out.reason     = PadDriverReason::Unsupported;
        out.want_claim = in.wanted && in.address_valid;
        return out;
    }

    if (!in.address_valid) {
        out.reason = PadDriverReason::NoAddress;
        return out;
    }

    if (!in.wanted) {
        out.reason = PadDriverReason::NotWanted;
        return out;
    }

    out.want_claim = true;

    if (!in.owns_output) {
        out.reason = PadDriverReason::Unsupported;
        return out;
    }

    out.driver         = PadDriver::Akira;
    out.reason         = PadDriverReason::Native;
    out.write_state    = true;
    out.stream_haptics = in.haptics_wanted && in.haptics_landing;

    return out;
}

} // namespace akira::platform::extended
