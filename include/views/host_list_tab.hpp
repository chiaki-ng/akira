#ifndef AKIRA_HOST_LIST_TAB_HPP
#define AKIRA_HOST_LIST_TAB_HPP

#include <borealis.hpp>
#include <unordered_map>

#include "core/discovery_manager.hpp"
#include "core/settings_manager.hpp"
#include "views/psn_action_button.hpp"
#include "views/profile_chip_view.hpp"
#if AKIRA_CLOUD
#include "views/cloud_shortcuts_rail.hpp"
#endif
#include "views/add_host_tab.hpp"

class HostItemView;

class HostListTab : public brls::Box {
    friend class HostItemView;

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

private:
    BRLS_BIND(brls::Box, hostContainer, "host/container");
    BRLS_BIND(brls::Box, emptyMessage, "empty/message");
    BRLS_BIND(brls::Box, chipSlot, "host/chipSlot");
    BRLS_BIND(brls::Box, railSlot, "host/railSlot");
    BRLS_BIND(brls::Button, findRemoteBtn, "host/findRemoteBtn");

    ProfileChipView* profileChip = nullptr;
#if AKIRA_CLOUD
    CloudShortcutsRail* shortcutsRail = nullptr;
#endif
    brls::Box* emptyActionCard = nullptr;

    void initFindRemoteButton();
    void applyProfileMode();

    static void connectToHost(Host* host);

    PsnActionButton findRemoteGate;

    SettingsManager* settings = nullptr;
    DiscoveryManager* discovery = nullptr;

    bool entrancePlayed = false;

    std::unordered_map<Host*, HostItemView*> hostItems;

    static HostListTab* currentInstance;
    static bool isConnecting;
    static bool isRegistering;
    static bool isActive;
    static bool connectionActive;
};

#endif // AKIRA_HOST_LIST_TAB_HPP
