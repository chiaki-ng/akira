#ifndef AKIRA_ADD_HOST_TAB_HPP
#define AKIRA_ADD_HOST_TAB_HPP

#include <borealis.hpp>

#include "settings/settings_manager.hpp"

namespace akira::consoles::views {

class AddHostTab : public brls::Box {
public:
    AddHostTab();

    static brls::View* create();

private:
    BRLS_BIND(brls::InputCell, hostNameInput, "add/hostName");
    BRLS_BIND(brls::InputCell, hostAddrInput, "add/hostAddr");
    BRLS_BIND(brls::SelectorCell, targetSelector, "add/target");
    BRLS_BIND(brls::Button, saveBtn, "add/saveBtn");
    BRLS_BIND(brls::Label, statusLabel, "add/status");

    settings::SettingsManager* settings = nullptr;

    void onSaveClicked();
};

}

#endif // AKIRA_ADD_HOST_TAB_HPP
