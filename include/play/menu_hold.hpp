#pragma once

#include <chrono>

namespace akira::play {

class MenuHold {
public:
    using Clock = std::chrono::steady_clock;

    static constexpr std::chrono::seconds holdTime{3};

    bool update(bool pressed, Clock::time_point now);
    bool held() const { return holding; }
    std::chrono::seconds heldFor(Clock::time_point now) const;

private:
    Clock::time_point start;
    bool holding = false;
};

}
