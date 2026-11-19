#include "platform/updates/github/settings_updates_view.hpp"

#include <borealis/core/i18n.hpp>
#include <string>
#include <vector>

#include "platform/updates/github/update_manager.hpp"
#include "app/version.hpp"
#include "util/http_pool.hpp"
#include "app/app_settings.hpp"
#include "settings/developer_settings.hpp"
#include "settings/settings_store.hpp"

namespace akira::platform::github
{

using namespace brls::literals;

SettingsUpdatesView::SettingsUpdatesView(const UpdateFlow& flow, UpdateFlow::Services services)
    : flow(flow), services(services) {
    this->inflateFromXMLRes("xml/settings/updates.xml");

    versionLabel->setText(std::string("Akira ") + app::version::string() +
                          " (" + app::version::channel() + ")");

    initChannelSelector();
    initAutoCheckToggle();
    initCheckNow();
}

SettingsUpdatesView::~SettingsUpdatesView() {
    *alive = false;
}

void SettingsUpdatesView::initChannelSelector() {
    bool powerUser = services.developer.powerUserMenuUnlocked();

    std::vector<std::string> options = { "akira/update/channel_stable"_i18n };
    std::vector<std::string> codes = { "stable" };
    if (powerUser) {
        options.push_back("akira/update/channel_rc"_i18n);
        codes.push_back("rc");
    }

    std::string current = services.app.updateChannel();
    int currentIndex = 0;
    for (size_t i = 0; i < codes.size(); i++) {
        if (codes[i] == current) {
            currentIndex = static_cast<int>(i);
            break;
        }
    }

    channelSelector->init(
        "akira/update/channel"_i18n,
        options,
        currentIndex,
        [](int) {},
        [this, codes](int selected) {
            std::string code = (selected >= 0 && selected < static_cast<int>(codes.size()))
                                   ? codes[selected]
                                   : std::string("stable");
            services.app.setUpdateChannel(code);
            services.store.save();
        });
}

void SettingsUpdatesView::initAutoCheckToggle() {
    autoCheckToggle->init(
        "akira/update/auto_check"_i18n,
        services.app.autoCheckUpdates(),
        [this](bool isOn) {
            services.app.setAutoCheckUpdates(isOn);
            services.store.save();
        });
}

void SettingsUpdatesView::initCheckNow() {
    checkNowCell->setText("akira/update/check_now"_i18n);
    checkNowCell->registerClickAction([this](brls::View*) {
        runCheck();
        return true;
    });
}

void SettingsUpdatesView::runCheck() {
    if (checking)
        return;
    checking = true;
    checkNowCell->setDetailText("akira/update/checking"_i18n);

    std::string channel = services.app.updateChannel();

    auto guard = alive;
    UpdateManager& manager = services.manager;
    services.http.submit([this, guard, channel, &manager](HttpSession&) {
        UpdateInfo info = manager.checkForUpdate(channel);

        brls::sync([this, guard, info]() {
            if (!*guard)
                return;
            checking = false;

            if (!info.error.empty()) {
                checkNowCell->setDetailText("akira/update/failed"_i18n);
                brls::Application::notify("akira/update/failed"_i18n);
                return;
            }

            if (info.available) {
                checkNowCell->setDetailText(info.version);
                flow.promptUpdate(info);
            } else {
                checkNowCell->setDetailText("akira/update/up_to_date"_i18n);
                brls::Application::notify("akira/update/up_to_date"_i18n);
            }
        });
    });
}

}

