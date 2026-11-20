#pragma once

#include <arisa/composition/observable.hpp>

#include "play/stats_hud_placement.hpp"
#include "play/stats_readout.hpp"
#include "play/stream_stats.hpp"

namespace akira::play::ui {

class StatsBoard {
public:
    struct View {
        StatsOverlayMode mode = StatsOverlayMode::Off;
        StatsReadout readout;
        StatsHudPlacement::Point origin;

        friend bool operator==(const View&, const View&) = default;
    };

    auto view() const -> const ::arisa::composition::Observable<View>& { return state; }

    void show(StatsOverlayMode mode);
    void refresh(StatsReadout readout);
    void place(StatsHudPlacement::Point origin);

private:
    ::arisa::composition::Observable<View> state;
};

}
