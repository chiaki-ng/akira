#pragma once

#include <cstdint>

namespace akira::play {

class ButtonCapture {
public:
    virtual ~ButtonCapture() = default;

    virtual void begin() = 0;
    virtual auto held() -> std::uint64_t = 0;
    virtual void end() = 0;
};

}
