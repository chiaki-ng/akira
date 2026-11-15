#pragma once

#include <memory>

#include "platform/stats/provider.hpp"


namespace akira::platform::nxext
{

class IpcStatsService;

class NxextStatsProvider final : public StatsProvider
{
public:
    NxextStatsProvider();
    ~NxextStatsProvider() override;

    void initToggle(brls::BooleanCell& cell) const override;
    void streamStarted(Session& session) override;
    void streamStopped() override;

private:
    std::unique_ptr<IpcStatsService> m_ipc_service;
};

}
