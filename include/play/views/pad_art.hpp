#ifndef AKIRA_INPUT_PAD_ART_HPP
#define AKIRA_INPUT_PAD_ART_HPP

#include "platform/pads/pad_art.hpp"

namespace akira::play::views {

inline const char* PadArtPath(const platform::PadDescription& pad)
{
    return platform::PadArt::path(pad);
}

inline bool PadHasTwoBatteries(const platform::PadDescription& pad)
{
    return platform::PadArt::twoBatteries(pad);
}

} // namespace akira::play::views

#endif // AKIRA_INPUT_PAD_ART_HPP
