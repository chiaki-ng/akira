#include "platform/nx/nx_platform.hpp"

#include <array>

#include <borealis.hpp>

#include "core/settings_manager.hpp"
#include "platform/nx/ipc_service.hpp"
#include "platform/nx/settings_updates_view.hpp"
#include "platform/nx/update_flow.hpp"
#include "platform/nx/update_manager.hpp"

using namespace brls::literals;

namespace akira::platform
{

Platform& Platform::instance()
{
    static NxPlatform platform;
    return platform;
}

NxPlatform::NxPlatform() = default;

NxPlatform::~NxPlatform() = default;

void NxPlatform::beforeInit(int argc, char* argv[])
{
    UpdateManager::setSelfPath(argc > 0 && argv[0] ? argv[0] : "");
}

void NxPlatform::launched()
{
    UpdateFlow::checkOnLaunch();
}

std::span<const char* const> NxPlatform::helpFiles() const
{
    static constexpr auto files = std::to_array<const char*>({"updates"});
    return files;
}

std::vector<Platform::SettingsMenu> NxPlatform::settingsMenus() const
{
    return {{"akira/settings/menu_updates"_i18n, []() -> brls::Box* { return new SettingsUpdatesView(); }}};
}

void NxPlatform::initIpcStatsToggle(brls::BooleanCell& cell) const
{
    auto* settings = SettingsManager::getInstance();
    cell.init(
        "akira/settings/ipc_stats"_i18n,
        settings->getIpcStatsEnabled(),
        [settings](bool isOn) {
            settings->setIpcStatsEnabled(isOn);
            settings->writeFile();
            brls::Logger::info("IPC Stats set to {}", isOn ? "true" : "false");
        });
}

void NxPlatform::initSimulateUpdate(brls::DetailCell& cell) const
{
    cell.setText("akira/settings/simulate_update"_i18n);
    cell.registerClickAction([](brls::View*) {
        UpdateFlow::simulate();
        return true;
    });
}

void NxPlatform::streamStarted(Session& session)
{
    if (!SettingsManager::getInstance()->getIpcStatsEnabled())
        return;
    if (!m_ipc_service)
    {
        m_ipc_service = std::make_unique<IpcStatsService>(&session);
        m_ipc_service->Start();
    }
    m_ipc_service->SetStreamActive(true);
}

void NxPlatform::streamStopped()
{
    if (m_ipc_service)
        m_ipc_service->SetStreamActive(false);
}

}
