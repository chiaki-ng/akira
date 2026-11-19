#ifndef AKIRA_SETTINGS_UPDATES_VIEW_HPP
#define AKIRA_SETTINGS_UPDATES_VIEW_HPP

#include <atomic>
#include <memory>

#include <borealis.hpp>
#include <borealis/views/cells/cell_bool.hpp>
#include <borealis/views/cells/cell_detail.hpp>
#include <borealis/views/cells/cell_selector.hpp>

#include "platform/updates/github/update_flow.hpp"

namespace akira::platform::github
{

class SettingsUpdatesView : public brls::Box {
public:
    SettingsUpdatesView(const UpdateFlow& flow, UpdateFlow::Services services);
    ~SettingsUpdatesView() override;

private:
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);

    BRLS_BIND(brls::SelectorCell, channelSelector, "settings/updateChannel");
    BRLS_BIND(brls::BooleanCell, autoCheckToggle, "settings/autoCheckUpdates");
    BRLS_BIND(brls::DetailCell, checkNowCell, "settings/checkForUpdates");
    BRLS_BIND(brls::Label, versionLabel, "settings/updateCurrentVersion");

    const UpdateFlow& flow;
    UpdateFlow::Services services;
    bool checking = false;

    void initChannelSelector();
    void initAutoCheckToggle();
    void initCheckNow();
    void runCheck();
};


}

#endif // AKIRA_SETTINGS_UPDATES_VIEW_HPP
