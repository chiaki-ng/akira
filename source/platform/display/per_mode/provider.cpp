#include "platform/display/per_mode/provider.hpp"
#include "log/log.hpp"

#include <switch.h>

#include "util/crash_report.hpp"

namespace akira::platform::per_mode
{

void PerModeDisplayProvider::prepare()
{
    CrashReport::stage("window size");
    if (const Result rc = nwindowSetModeDimensions(nwindowGetDefault(), 0, 0, 0, 0); R_FAILED(rc))
        akira::log::warning("switch: per-mode window size refused (0x{:x}), drawing at the default window size", rc);
}

void PerModeDisplayProvider::streaming(bool active)
{
    NWindow* window = nwindowGetDefault();
    const Result rc = active ? nwindowSetFrameRates(window, 0, 0, true) : nwindowSetSwapInterval(window, 1);
    if (R_FAILED(rc))
        akira::log::warning("switch: {} refresh rate refused (0x{:x})", active ? "stream" : "menu", rc);
}

}
