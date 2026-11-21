#include "play/ui/stats_board.hpp"

#include <utility>

namespace akira::play::ui {

void StatsBoard::show(DiagnosticsAppearance appearance)
{
    state.write(&View::appearance, appearance);
}

void StatsBoard::refresh(DiagnosticsReport report)
{
    state.write(&View::report, std::move(report));
}

void StatsBoard::place(StatsHudPlacement::Point origin)
{
    state.write(&View::origin, origin);
}

}
