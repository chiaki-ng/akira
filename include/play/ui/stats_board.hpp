#pragma once

#include <arisa/composition/observable.hpp>

#include "play/diagnostics_appearance.hpp"
#include "play/diagnostics_report.hpp"
#include "play/stats_hud_placement.hpp"

namespace akira::play::ui {

class StatsBoard {
public:
    struct View {
        DiagnosticsAppearance appearance;
        DiagnosticsReport report;
        StatsHudPlacement::Point origin;
        float scale = 1.0F;

        friend bool operator==(const View&, const View&) = default;
    };

    auto view() const -> const ::arisa::composition::Observable<View>& { return state; }

    void show(DiagnosticsAppearance appearance);
    void refresh(DiagnosticsReport report);
    void place(StatsHudPlacement::Point origin, float scale);

private:
    ::arisa::composition::Observable<View> state;
};

}
