#ifndef AKIRA_SETTINGS_UPDATES_VIEW_HPP
#define AKIRA_SETTINGS_UPDATES_VIEW_HPP

#include <atomic>
#include <memory>

#include <borealis.hpp>
#include <borealis/views/cells/cell_bool.hpp>
#include <borealis/views/cells/cell_detail.hpp>
#include <borealis/views/cells/cell_selector.hpp>

#include "core/settings_manager.hpp"

class SettingsUpdatesView : public brls::Box {
public:
    SettingsUpdatesView();
    ~SettingsUpdatesView() override;

private:
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);

    BRLS_BIND(brls::SelectorCell, channelSelector, "settings/updateChannel");
    BRLS_BIND(brls::BooleanCell, autoCheckToggle, "settings/autoCheckUpdates");
    BRLS_BIND(brls::DetailCell, checkNowCell, "settings/checkForUpdates");
    BRLS_BIND(brls::Label, versionLabel, "settings/updateCurrentVersion");

    SettingsManager* settings = nullptr;
    bool checking = false;

    void initChannelSelector();
    void initAutoCheckToggle();
    void initCheckNow();
    void runCheck();
};

#endif // AKIRA_SETTINGS_UPDATES_VIEW_HPP
