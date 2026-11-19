#pragma once

#include <cstddef>
#include <vector>

#include "play/stream_stats.hpp"

namespace akira::play {

class StreamControls {
public:
    enum class Control { Power, Stats, Pad, Players, Motion };

    struct Action {
        enum class Kind { None, Activate, Choose };
        Kind kind = Kind::None;
        std::size_t control = 0;
        int option = -1;
        bool dismiss = false;
    };

    StreamControls();

    void offerSleep(bool available);
    void showStats(StatsOverlayMode mode);
    void offerPlayers();
    void enablePlayers(bool available);

    const std::vector<Control>& controls() const { return shown; }
    std::size_t selection() const { return selected; }
    int optionCursor() const { return cursor; }
    bool inPanel() const { return panelOpen; }
    StatsOverlayMode stats() const { return statsMode; }
    bool offersSleep() const { return sleepAvailable; }

    bool available(std::size_t index) const;
    bool instant(std::size_t index) const;
    bool closesMenu(std::size_t index) const;
    int optionCount(std::size_t index) const;
    int optionIndex(std::size_t index) const;

    void moveSelection(int delta);
    void moveOption(int delta);
    void enterPanel();
    void leavePanel();
    [[nodiscard]] bool back();
    Action fire();
    Action tapControl(std::size_t index);
    Action tapOption(int option);

private:
    void rebuild();
    bool valid(std::size_t index) const { return index < shown.size(); }
    int startingOption(std::size_t index) const;
    Action activate(std::size_t index) const;
    Action choose(std::size_t index, int option);

    std::vector<Control> shown;
    std::size_t selected = 0;
    int cursor = -1;
    bool panelOpen = false;
    bool sleepAvailable = true;
    bool playersOffered = false;
    bool playersAvailable = true;
    StatsOverlayMode statsMode = StatsOverlayMode::Off;
};

}
