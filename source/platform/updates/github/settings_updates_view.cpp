#include "platform/updates/github/settings_updates_view.hpp"

#include <borealis/core/i18n.hpp>
#include <string>
#include <vector>

#include "platform/updates/github/update_manager.hpp"
#include "app/version.hpp"
#include "util/http_pool.hpp"
#include "platform/updates/github/update_flow.hpp"

namespace akira::platform::github
{

using namespace brls::literals;

SettingsUpdatesView::SettingsUpdatesView() {
    this->inflateFromXMLRes("xml/settings/updates.xml");

    settings = SettingsManager::getInstance();

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
    bool powerUser = settings->getPowerUserMenuUnlocked();

    std::vector<std::string> options = { "akira/update/channel_stable"_i18n };
    std::vector<std::string> codes = { "stable" };
    if (powerUser) {
        options.push_back("akira/update/channel_rc"_i18n);
        codes.push_back("rc");
    }

    std::string current = settings->getUpdateChannel();
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
            settings->setUpdateChannel(code);
            settings->writeFile();
        });
}

void SettingsUpdatesView::initAutoCheckToggle() {
    autoCheckToggle->init(
        "akira/update/auto_check"_i18n,
        settings->getAutoCheckUpdates(),
        [this](bool isOn) {
            settings->setAutoCheckUpdates(isOn);
            settings->writeFile();
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

    std::string channel = settings->getUpdateChannel();

    auto guard = alive;
    HttpPool::instance().submit([this, guard, channel](HttpSession&) {
        UpdateInfo info = UpdateManager::getInstance().checkForUpdate(channel);

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
                UpdateFlow::promptUpdate(info);
            } else {
                checkNowCell->setDetailText("akira/update/up_to_date"_i18n);
                brls::Application::notify("akira/update/up_to_date"_i18n);
            }
        });
    });
}

}

