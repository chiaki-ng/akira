#include "play/ui/stats_board.hpp"

#include <utility>

namespace akira::play::ui {

void StatsBoard::show(StatsOverlayMode mode)
{
    state.write(&View::mode, mode);
}

void StatsBoard::refresh(StatsReadout readout)
{
    state.write(&View::readout, std::move(readout));
}

void StatsBoard::place(StatsHudPlacement::Point origin)
{
    state.write(&View::origin, origin);
}

}
