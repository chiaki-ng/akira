#include "platform/stats/nxext/provider.hpp"
#include "log/log.hpp"

#include <borealis.hpp>

#include "settings/developer_settings.hpp"
#include "settings/settings_store.hpp"
#include "platform/stats/nxext/ipc_service.hpp"

using namespace brls::literals;

namespace akira::platform::nxext
{

NxextStatsProvider::NxextStatsProvider(settings::DeveloperSettings& developer, settings::SettingsStore& store,
    const network::WireGuardManager& wireGuard)
    : m_developer(developer)
    , m_store(store)
    , m_wire_guard(wireGuard)
{
}

NxextStatsProvider::~NxextStatsProvider() = default;

void NxextStatsProvider::initToggle(brls::BooleanCell& cell) const
{
    cell.init(
        "akira/settings/ipc_stats"_i18n,
        m_developer.ipcStatsEnabled(),
        [&developer = m_developer, &store = m_store](bool isOn) {
            developer.setIpcStatsEnabled(isOn);
            store.save();
            log::info("IPC Stats set to {}", isOn ? "true" : "false");
        });
}

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
