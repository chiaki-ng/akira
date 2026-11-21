#include "play/screenshot_chord.hpp"

namespace akira::play {

auto ScreenshotChord::update(std::uint64_t held) -> bool
{
    const bool now = (held & chord) == chord;
    const bool fired = now && !complete;
    complete = now;
    if (fired)
        swallowed = true;
    if ((held & stick) == 0)
        swallowed = false;
    return fired;
}

}
