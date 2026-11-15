#include "platform/display/per_mode/provider.hpp"

#include <borealis.hpp>
#include <switch.h>

#include "util/crash_report.hpp"

namespace akira::platform::per_mode
{

void PerModeDisplayProvider::prepare()
{
    CrashReport::stage("window size");
    if (const Result rc = nwindowSetModeDimensions(nwindowGetDefault(), 0, 0, 0, 0); R_FAILED(rc))
        brls::Logger::warning("switch: per-mode window size refused (0x{:x}), drawing at the default window size", rc);
}

}
