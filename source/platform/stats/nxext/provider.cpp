#include "platform/stats/nxext/provider.hpp"

#include "settings/developer_settings.hpp"
#include "platform/stats/nxext/ipc_service.hpp"

namespace akira::platform::nxext
{

NxextStatsProvider::NxextStatsProvider(const settings::DeveloperSettings& developer,
    const network::WireGuardManager& wireGuard)
    : m_developer(developer)
    , m_wire_guard(wireGuard)
{
}

NxextStatsProvider::~NxextStatsProvider() = default;

void NxextStatsProvider::streamStarted(play::Session& session)
{
    if (!m_developer.ipcStatsEnabled())
        return;
    if (!m_ipc_service)
    {
        m_ipc_service = std::make_unique<IpcStatsService>(&session, m_wire_guard);
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
