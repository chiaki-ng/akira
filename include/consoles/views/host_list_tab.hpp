#ifndef AKIRA_HOST_LIST_TAB_HPP
#define AKIRA_HOST_LIST_TAB_HPP

#include <borealis.hpp>
#include <optional>
#include <unordered_map>

#include "consoles/console_actions.hpp"
#include "consoles/console_list.hpp"
#include "consoles/debug_consoles.hpp"
#include "consoles/discovery_manager.hpp"
#include "app/view_services.hpp"
#include "accounts/views/psn_action_button.hpp"
#include "accounts/views/profile_chip_view.hpp"
#if AKIRA_CLOUD
#include "cloud/views/cloud_shortcuts_rail.hpp"
#endif
#include "consoles/views/add_host_tab.hpp"

namespace akira::consoles::views {

class ConsoleCard;

class HostListTab : public brls::Box {
public:
    HostListTab();
    ~HostListTab() override;

    static brls::View* create();

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;

    brls::View* getDefaultFocus() override;

    void updateHostItem(Host* host);

    void syncHostList();

    static void notifyActiveProfileChanged();
    static void notifyAccountNameDisplayChanged();

    static void refreshRailsIfActive();

    static void connectToHost(Host* host);
    static ConsoleActions actions();
    static ConsoleList consoleList();

    static void consoleChanged(Host* host);
    static void consoleLinked();
    static void consoleRemoved();

private:
    static const brls::ButtonStyle findRemoteStyle;

    BRLS_BIND(brls::Box, hostContainer, "host/container");
    BRLS_BIND(brls::Box, emptyMessage, "empty/message");
    BRLS_BIND(brls::Box, chipSlot, "host/chipSlot");
    BRLS_BIND(brls::Box, railSlot, "host/railSlot");
    BRLS_BIND(brls::Button, findRemoteBtn, "host/findRemoteBtn");

    accounts::views::ProfileChipView* profileChip = nullptr;
#if AKIRA_CLOUD
    cloud::views::CloudShortcutsRail* shortcutsRail = nullptr;
#endif
    brls::Box* emptyActionCard = nullptr;

    void initFindRemoteButton();
    void applyProfileMode();

    static void refreshSurfaces();

    accounts::views::PsnActionButton findRemoteGate;

    app::Services* services = nullptr;
    DiscoveryManager* discovery = nullptr;

    bool entrancePlayed = false;

    std::unordered_map<Host*, ConsoleCard*> hostItems;
    std::optional<DebugConsoles> debugConsoles;

    static HostListTab* currentInstance;
};

}

#endif // AKIRA_HOST_LIST_TAB_HPP
