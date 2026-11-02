#ifndef AKIRA_SETTINGS_DEVELOPER_VIEW_HPP
#define AKIRA_SETTINGS_DEVELOPER_VIEW_HPP

#include <borealis.hpp>
#include <borealis/views/cells/cell_bool.hpp>
#include <borealis/views/cells/cell_detail.hpp>
#include <borealis/views/cells/cell_selector.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/settings_manager.hpp"

class SettingsDeveloperView : public brls::Box {
public:
    SettingsDeveloperView();
    ~SettingsDeveloperView() override;

private:
    BRLS_BIND(brls::BooleanCell, fakeHostsToggle, "settings/devFakeHosts");
    BRLS_BIND(brls::SelectorCell, takionPs4Selector, "settings/devTakionVersionPs4");
    BRLS_BIND(brls::SelectorCell, takionPs5Selector, "settings/devTakionVersionPs5");
    BRLS_BIND(brls::SelectorCell, takionPs5CloudSelector, "settings/devTakionVersionPs5Cloud");
    BRLS_BIND(brls::SelectorCell, takionPsNowSelector, "settings/devTakionVersionPsNow");
    BRLS_BIND(brls::DetailCell, simulateUpdateCell, "settings/simulateUpdate");
    BRLS_BIND(brls::DetailCell, tlsVerifyProbeCell, "settings/tlsVerifyProbe");
    BRLS_BIND(brls::DetailCell, logBrowserCell, "settings/logBrowser");

    void initTakionSelector(brls::SelectorCell* cell, TakionTarget target, std::string title,
        std::vector<std::uint32_t> implemented);

    static constexpr std::uint32_t highestCloudTakion = 23;

    SettingsManager* settings = nullptr;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
};

#endif // AKIRA_SETTINGS_DEVELOPER_VIEW_HPP
