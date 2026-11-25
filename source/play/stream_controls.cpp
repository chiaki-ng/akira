#include "play/stream_controls.hpp"

namespace akira::play {

StreamControls::StreamControls()
{
    rebuild();
}

void StreamControls::offerSleep(bool available)
{
    sleepAvailable = available;
    rebuild();
}

void StreamControls::showStats(StatsOverlayMode mode)
{
    statsMode = mode;
    rebuild();
}

void StreamControls::offerPlayers()
{
    playersOffered = true;
    rebuild();
}

void StreamControls::enablePlayers(bool available)
{
    playersAvailable = available;
    rebuild();
}

void StreamControls::rebuild()
{
    shown = {Control::Power, Control::Stats, Control::Pad};
    if (playersOffered)
        shown.push_back(Control::Players);
    shown.push_back(Control::Motion);
}

bool StreamControls::available(std::size_t index) const
{
    if (!valid(index))
        return false;
    return shown[index] != Control::Players || playersAvailable;
}

bool StreamControls::instant(std::size_t index) const
{
    if (!valid(index))
        return false;
    const Control control = shown[index];
    return control == Control::Pad || control == Control::Players || control == Control::Motion;
}

bool StreamControls::closesMenu(std::size_t index) const
{
    if (!valid(index))
        return false;
    return shown[index] == Control::Stats || shown[index] == Control::Motion;
}

int StreamControls::optionCount(std::size_t index) const
{
    if (!valid(index))
        return 0;
    switch (shown[index]) {
        case Control::Power: return sleepAvailable ? 2 : 1;
        case Control::Stats: return 3;
        default: return 0;
    }
}

int StreamControls::optionIndex(std::size_t index) const
{
    if (valid(index) && shown[index] == Control::Stats)
        return static_cast<int>(statsMode);
    return -1;
}

int StreamControls::startingOption(std::size_t index) const
{
    return optionIndex(index) >= 0 ? optionIndex(index) : 0;
}

void StreamControls::moveSelection(int delta)
{
    if (shown.empty())
        return;

    int n = static_cast<int>(shown.size());
    int i = static_cast<int>(selected);
    for (int step = 0; step < n; step++)
    {
        i = (i + delta + n) % n;
        if (available(static_cast<std::size_t>(i)))
            break;
    }
    selected = static_cast<std::size_t>(i);
    cursor = -1;
}

void StreamControls::moveOption(int delta)
{
    if (!valid(selected))
        return;

    int n = optionCount(selected);
    if (n <= 0)
        return;

    if (cursor < 0)
        cursor = startingOption(selected);

    cursor = (cursor + delta + n) % n;
}

void StreamControls::enterPanel()
{
    if (!valid(selected) || optionCount(selected) <= 0)
        return;

    panelOpen = true;
    if (cursor < 0)
        cursor = startingOption(selected);
}

void StreamControls::leavePanel()
{
    panelOpen = false;
}

bool StreamControls::back()
{
    if (panelOpen)
    {
        leavePanel();
        return false;
    }
    return true;
}

StreamControls::Action StreamControls::fire()
{
    if (!valid(selected) || !available(selected))
        return {};

    if (panelOpen && optionCount(selected) > 0)
        return choose(selected, cursor < 0 ? 0 : cursor);

    if (instant(selected))
        return activate(selected);

    enterPanel();
    return {};
}

StreamControls::Action StreamControls::tapControl(std::size_t index)
{
    if (!valid(index))
        return {};

    if (instant(index))
    {
        selected = index;
        return activate(index);
    }

    if (selected != index)
    {
        selected = index;
        cursor = -1;
    }
    panelOpen = true;
    if (cursor < 0)
        cursor = startingOption(index);
    return {};
}

StreamControls::Action StreamControls::tapOption(int option)
{
    if (!valid(selected))
        return {};
    panelOpen = true;
    cursor = option;
    return choose(selected, option);
}

StreamControls::Action StreamControls::activate(std::size_t index) const
{
    return Action{.kind = Action::Kind::Activate, .control = index, .dismiss = closesMenu(index)};
}

StreamControls::Action StreamControls::choose(std::size_t index, int option)
{
    if (shown[index] == Control::Stats)
        statsMode = option == 1 ? StatsOverlayMode::Compact
                  : option == 2 ? StatsOverlayMode::Full
                                : StatsOverlayMode::Off;
    return Action{.kind = Action::Kind::Choose, .control = index, .option = option, .dismiss = closesMenu(index)};
}

}
