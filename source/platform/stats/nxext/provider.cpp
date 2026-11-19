#include "platform/stats/nxext/provider.hpp"
#include "log/log.hpp"

#include <borealis.hpp>

#include "settings/settings_manager.hpp"
#include "platform/stats/nxext/ipc_service.hpp"

using namespace brls::literals;

namespace akira::platform::nxext
{

NxextStatsProvider::NxextStatsProvider() = default;

NxextStatsProvider::~NxextStatsProvider() = default;

void NxextStatsProvider::initToggle(brls::BooleanCell& cell) const
{
    auto* settings = SettingsManager::getInstance();
    cell.init(
        "akira/settings/ipc_stats"_i18n,
        settings->getIpcStatsEnabled(),
        [settings](bool isOn) {
            settings->setIpcStatsEnabled(isOn);
            settings->writeFile();
            akira::log::info("IPC Stats set to {}", isOn ? "true" : "false");
        });
}

void NxextStatsProvider::streamStarted(play::Session& session)
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

void NxextStatsProvider::streamStopped()
{
    if (m_ipc_service)
        m_ipc_service->SetStreamActive(false);
}

}
