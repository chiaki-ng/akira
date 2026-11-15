#include "platform/ounce/ounce_platform.hpp"

#include <array>

#include <borealis.hpp>
#include <switch.h>

#include "platform/ounce/ounce_haptics.hpp"
#include "platform/ounce/ounce_pad_feedback.hpp"
#include "platform/ounce/ounce_pad_settings.hpp"
#include "platform/ounce/ounce_stream_controls.hpp"
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

OuncePadSource& OuncePlatform::pads()
{
    return m_pads;
}

std::unique_ptr<PadHaptics> OuncePlatform::createHaptics()
{
    return std::make_unique<OunceHaptics>();
}

std::unique_ptr<PadFeedback> OuncePlatform::createFeedback(HapticManager& haptics)
{
    return std::make_unique<OuncePadFeedback>(haptics);
}

std::unique_ptr<StreamControls> OuncePlatform::createStreamControls(std::weak_ptr<StreamControlsHost> host)
{
    return std::make_unique<OunceStreamControls>(std::move(host));
}

std::unique_ptr<PadSettings> OuncePlatform::createPadSettings(const input::PadDescription& pad)
{
    return std::make_unique<OuncePadSettings>(pad);
}

}
