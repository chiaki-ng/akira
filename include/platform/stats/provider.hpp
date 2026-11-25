#pragma once

namespace akira::play {
class Session;
}

namespace akira::platform
{

class StatsProvider
{
public:
    virtual ~StatsProvider() = default;

    virtual void streamStarted(play::Session& session) = 0;
    virtual void streamStopped() = 0;
};

}
