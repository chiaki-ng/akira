#ifndef AKIRA_SETTINGS_CONTROLLER_VIEW_HPP
#define AKIRA_SETTINGS_CONTROLLER_VIEW_HPP

#include <borealis.hpp>
#include <borealis/views/cells/cell_bool.hpp>
#include <borealis/views/cells/cell_detail.hpp>
#include <borealis/views/cells/cell_selector.hpp>
#include <borealis/views/cells/cell_slider.hpp>

#include "app/view_services.hpp"

namespace akira::settings::views {

class SettingsControllerView : public brls::Box {
public:
    SettingsControllerView();

private:
    BRLS_BIND(brls::BooleanCell, sleepOnExitToggle, "settings/sleepOnExit");
    BRLS_BIND(brls::DetailCell, connectedControllersCell, "settings/connectedControllers");
    BRLS_BIND(brls::DetailCell, buttonMappingCell, "settings/buttonMapping");

    app::Services* services = nullptr;

    void initSleepOnExitToggle();
    void initConnectedControllers();
    void initButtonMappingCell();
};

}

#endif // AKIRA_SETTINGS_CONTROLLER_VIEW_HPP
