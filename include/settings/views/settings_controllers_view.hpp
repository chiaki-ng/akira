#ifndef AKIRA_SETTINGS_CONTROLLERS_VIEW_HPP
#define AKIRA_SETTINGS_CONTROLLERS_VIEW_HPP

#include <borealis.hpp>
#include <borealis/views/cells/cell_detail.hpp>

#include <string>

#include "platform/pads/pad.hpp"

class SettingsControllersView : public brls::Box {
public:
    SettingsControllersView();

private:
    void rebuild();

    static std::string tierSummary(const akira::input::PadDescription& pad);

    brls::Box* list = nullptr;
};

#endif // AKIRA_SETTINGS_CONTROLLERS_VIEW_HPP
