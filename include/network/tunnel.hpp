#pragma once

#include <string>

namespace akira::network {

class Tunnel {
public:
    virtual ~Tunnel() = default;

    virtual auto configured() const -> bool = 0;
    virtual auto connected() const -> bool = 0;
    virtual auto address() const -> std::string = 0;
    virtual auto lastError() const -> std::string = 0;
    virtual auto connect() -> bool = 0;
    virtual void disconnect() = 0;
    virtual void restartSearch() = 0;
};

}
