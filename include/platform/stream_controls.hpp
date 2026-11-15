#pragma once

#include <memory>

class Host;
class Session;
class StreamMenu;

namespace akira::stream
{
class Connection;
}

namespace akira::platform
{

class StreamControlsHost
{
public:
    virtual ~StreamControlsHost() = default;

    virtual Session* controlsSession() = 0;
    virtual std::shared_ptr<stream::Connection> controlsConnection() = 0;
    virtual Host* controlsHost() = 0;
    virtual bool controlsMenuOpen() const = 0;

    virtual bool prepareController() = 0;
    virtual void continueStart() = 0;
    virtual void abandonStart() = 0;
    virtual void pauseForUi() = 0;
    virtual void resumeAfterUi() = 0;
};

class StreamControls
{
public:
    virtual ~StreamControls() = default;

    virtual void attach() = 0;
    virtual bool beforeStart() = 0;
    virtual void streaming() = 0;
    virtual void decorateMenu(StreamMenu& menu) = 0;
};

}
