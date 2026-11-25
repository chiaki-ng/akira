#pragma once

#include <string>

namespace akira::consoles {
class Host;
}

namespace akira::consoles::ui {

class ConsoleStarter {
public:
    virtual ~ConsoleStarter() = default;

    virtual void start(Host& target) = 0;
    virtual void prepare(const Host& target, std::string activity) = 0;
    virtual bool prepared(const Host& target) const = 0;
};

}
