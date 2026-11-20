#pragma once

#include <string>

namespace akira::app {

class Notifier {
public:
    virtual ~Notifier() = default;

    virtual void notify(std::string text) = 0;
};

}
