#pragma once

#include <memory>

#include "platform/stats/provider.hpp"


namespace akira::network
{
class WireGuardManager;
}

namespace akira::settings
{
class DeveloperSettings;
class SettingsStore;
}

namespace akira::platform::nxext
{

class IpcStatsService;

class NxextStatsProvider final : public StatsProvider
{
public:
    NxextStatsProvider(settings::DeveloperSettings& developer, settings::SettingsStore& store,
        const network::WireGuardManager& wireGuard);
    ~NxextStatsProvider() override;

    void initToggle(brls::BooleanCell& cell) const override;
    void streamStarted(play::Session& session) override;
    void streamStopped() override;

private:
    settings::DeveloperSettings& m_developer;
    settings::SettingsStore& m_store;
    const network::WireGuardManager& m_wire_guard;
    std::unique_ptr<IpcStatsService> m_ipc_service;
};

}
