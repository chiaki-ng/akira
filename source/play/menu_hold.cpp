#include "play/menu_hold.hpp"

namespace akira::play {

bool MenuHold::update(bool pressed, Clock::time_point now)
{
    if (!pressed) {
        holding = false;
        return false;
    }
    if (!holding) {
        start = now;
        holding = true;
        return false;
    }
    if (heldFor(now) < holdTime)
        return false;
    holding = false;
    return true;
}

std::chrono::seconds MenuHold::heldFor(Clock::time_point now) const
{
    if (!holding)
        return std::chrono::seconds{0};
    return std::chrono::duration_cast<std::chrono::seconds>(now - start);
}

}
