#pragma once

#include <memory>

namespace akira::consoles {
class Host;
}

namespace akira::play
{
class Connection;
class Session;
}

namespace akira::play::views
{

class StreamControlsHost
{
public:
    virtual ~StreamControlsHost() = default;

    virtual play::Session* controlsSession() = 0;
    virtual std::shared_ptr<play::Connection> controlsConnection() = 0;
    virtual consoles::Host* controlsHost() = 0;
    virtual bool controlsMenuOpen() const = 0;

    virtual bool prepareController() = 0;
    virtual void continueStart() = 0;
    virtual void abandonStart() = 0;
    virtual void pauseForUi() = 0;
    virtual void resumeAfterUi() = 0;
};

}
