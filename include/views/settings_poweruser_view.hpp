#ifndef AKIRA_SETTINGS_POWERUSER_VIEW_HPP
#define AKIRA_SETTINGS_POWERUSER_VIEW_HPP

#include <borealis.hpp>
#include <borealis/views/cells/cell_bool.hpp>
#include <borealis/views/cells/cell_slider.hpp>

#include "core/settings_manager.hpp"

class SettingsPowerUserView : public brls::Box {
public:
    SettingsPowerUserView();

private:
    BRLS_BIND(brls::BooleanCell, unlockBitrateMaxToggle, "settings/unlockBitrateMax");
    BRLS_BIND(brls::BooleanCell, ipcStatsToggle, "settings/ipcStats");
    BRLS_BIND(brls::BooleanCell, portGuessingToggle, "settings/portGuessing");
    BRLS_BIND(brls::SliderCell, portGuessingCountSlider, "settings/portGuessingCount");
    BRLS_BIND(brls::SliderCell, portGuessingSocksSlider, "settings/portGuessingSocks");
    BRLS_BIND(brls::BooleanCell, autoReconnectToggle, "settings/autoReconnect");
    BRLS_BIND(brls::BooleanCell, hideAccountNameToggle, "settings/hideAccountName");
    BRLS_BIND(brls::Button, runBenchmarkBtn, "settings/runBenchmark");
    BRLS_BIND(brls::Button, flushTrophyCacheBtn, "settings/flushTrophyCache");

    SettingsManager* settings = nullptr;

    void initUnlockBitrateMaxToggle();
    void initIpcStatsToggle();
    void initAutoReconnectToggle();
    void initHideAccountNameToggle();
    void initPortGuessingToggle();
    void initPortGuessingCountSlider();
    void initPortGuessingSocksSlider();
    void runGhashBenchmark();
};

#endif // AKIRA_SETTINGS_POWERUSER_VIEW_HPP
