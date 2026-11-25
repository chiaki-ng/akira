#pragma once

#include "platform/stats/provider.hpp"

namespace akira::platform::none
{

class NoStatsProvider final : public StatsProvider
{
public:
    void streamStarted(play::Session&) override {}
    void streamStopped() override {}
};

}
