#ifndef AKIRA_SETTINGS_GENERAL_VIEW_HPP
#define AKIRA_SETTINGS_GENERAL_VIEW_HPP

#include <borealis.hpp>
#include <borealis/views/cells/cell_bool.hpp>
#include <borealis/views/cells/cell_detail.hpp>
#include <borealis/views/cells/cell_input.hpp>
#include <borealis/views/cells/cell_selector.hpp>
#include <borealis/views/cells/cell_slider.hpp>
#include <chrono>

#include "core/settings_manager.hpp"

class SettingsGeneralView : public brls::Box {
public:
    SettingsGeneralView();

private:
    BRLS_BIND(brls::SelectorCell, languageSelector, "settings/language");
    BRLS_BIND(brls::SelectorCell, themeSelector, "settings/theme");
    BRLS_BIND(brls::Box, subnetsBox, "settings/subnetsBox");
    BRLS_BIND(brls::DetailCell, customDnsCell, "settings/customDns");
    brls::Box* addSubnetRow = nullptr;
    BRLS_BIND(brls::Label, versionLabel, "settings/version");
    BRLS_BIND(brls::BooleanCell, holepunchRetryToggle, "settings/holepunchRetry");
    BRLS_BIND(brls::BooleanCell, connectionShowStagesToggle, "settings/connectionShowStages");
    BRLS_BIND(brls::BooleanCell, requestIdrOnFecFailureToggle, "settings/requestIdrOnFecFailure");
    BRLS_BIND(brls::SliderCell, packetLossMaxSlider, "settings/packetLossMax");

    SettingsManager* settings = nullptr;
    int powerUserClickCount = 0;
    std::chrono::steady_clock::time_point lastPowerUserClick;

    void initLanguageSelector();
    void initThemeSelector();
    void rebuildSubnetsUI();
    void scheduleSubnetsRebuild();
    void initCustomDnsCell();
    void refreshCustomDnsCell();
    void promptCustomDns();
    void promptAddSubnet();
    void removeSubnet(const std::string& value);
    void initVersionUnlock();
    void initHolepunchRetryToggle();
    void initConnectionShowStagesToggle();
    void initRequestIdrOnFecFailureToggle();
    void initPacketLossMaxSlider();
};

#endif // AKIRA_SETTINGS_GENERAL_VIEW_HPP
