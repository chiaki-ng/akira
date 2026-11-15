#include "platform/ounce/ounce_platform.hpp"

#include <array>

#include <borealis.hpp>
#include <switch.h>

#include "util/crash_report.hpp"

namespace akira::platform
{

Platform& Platform::instance()
{
    static OuncePlatform platform;
    return platform;
}

void OuncePlatform::beforeInit(int, char*[])
{
    CrashReport::stage("window size");
    if (const Result rc = nwindowSetModeDimensions(nwindowGetDefault(), 0, 0, 0, 0); R_FAILED(rc))
        brls::Logger::warning("switch: per-mode window size refused (0x{:x}), drawing at the default window size", rc);
}

void OuncePlatform::launched()
{
}

std::span<const char* const> OuncePlatform::helpFiles() const
{
    static constexpr auto files = std::to_array<const char*>({"ounce"});
    return files;
}

std::vector<Platform::SettingsMenu> OuncePlatform::settingsMenus() const
{
    return {};
}

void OuncePlatform::initIpcStatsToggle(brls::BooleanCell& cell) const
{
    cell.setVisibility(brls::Visibility::GONE);
}

void OuncePlatform::initSimulateUpdate(brls::DetailCell& cell) const
{
    cell.setVisibility(brls::Visibility::GONE);
}

void OuncePlatform::streamStarted(Session&)
{
}

void OuncePlatform::streamStopped()
{
}

}
