#pragma once

namespace brls
{
class BooleanCell;
}

class Session;

namespace akira::platform
{

class StatsProvider
{
public:
    virtual ~StatsProvider() = default;

    virtual void initToggle(brls::BooleanCell& cell) const = 0;
    virtual void streamStarted(Session& session) = 0;
    virtual void streamStopped() = 0;
};

}
