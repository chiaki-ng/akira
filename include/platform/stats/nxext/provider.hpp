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
}

namespace akira::platform::nxext
{

class IpcStatsService;

class NxextStatsProvider final : public StatsProvider
{
public:
    NxextStatsProvider(const settings::DeveloperSettings& developer, const network::WireGuardManager& wireGuard);
    ~NxextStatsProvider() override;

    void streamStarted(play::Session& session) override;
    void streamStopped() override;

private:
    const settings::DeveloperSettings& m_developer;
    const network::WireGuardManager& m_wire_guard;
    std::unique_ptr<IpcStatsService> m_ipc_service;
};

}
