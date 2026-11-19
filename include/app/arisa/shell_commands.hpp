#pragma once

#include <string>

namespace akira::app::arisa {

class ShellCommands {
public:
    virtual ~ShellCommands() = default;

    virtual void connect(const std::string& catalogName) = 0;
    virtual void cancel() = 0;
    virtual void quit() = 0;
};

}
