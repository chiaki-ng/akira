#include "app/views/akira_header.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/views/hint.hpp>

#include <fstream>

#include "accounts/views/profile_setup_view.hpp"
#include "app/views/header_status_view.hpp"
#include "app/views/theme.hpp"
#include "log/log.hpp"
#include "settings/settings_manager.hpp"
#include "settings/views/settings_frame_view.hpp"
#include "trophies/views/trophy_list_tab.hpp"

using namespace brls::literals;

namespace akira::app::views {

std::string AkiraHeader::appVersion()
{
    std::ifstream file("romfs:/build_info.txt");
    if (!file.is_open()) {
        return "";
    }
    std::string line;
    if (std::getline(file, line)) {
        size_t spacePos = line.find(' ');
        if (spacePos != std::string::npos) {
            return "v" + line.substr(spacePos + 1);
        }
    }
    return "";
}

void AkiraHeader::decorate(brls::AppletFrame* appletFrame)
{
    if (!appletFrame)
        return;

    std::string version = appVersion();
    appletFrame->setTitle("Akira");
    if (!version.empty()) {
        if (auto* titleLabel = dynamic_cast<brls::Label*>(appletFrame->getView("brls/applet_frame/title_label"))) {
            titleLabel->setMarginTop(0);
            titleLabel->setFontSize(34);
            if (auto* titleBox = dynamic_cast<brls::Box*>(titleLabel->getParent())) {
                titleBox->setAlignItems(brls::AlignItems::BASELINE);

                auto* versionLabel = new brls::Label();
                versionLabel->setText(version);
                versionLabel->setFontSize(19);
                versionLabel->setTextColor(active().textDim);
                versionLabel->setMarginLeft(8);

                const auto& kids = titleBox->getChildren();
                size_t insertAt = kids.size();
                for (size_t i = 0; i < kids.size(); i++) {
                    if (kids[i] == titleLabel) {
                        insertAt = i + 1;
                        break;
                    }
                }
                titleBox->addView(versionLabel, insertAt);
            }
        }
    }

    auto* header = appletFrame->getHeader();
    if (header) {
        header->setAlignItems(brls::AlignItems::CENTER);

        auto* headerRight = new brls::Box();
        headerRight->setAxis(brls::Axis::COLUMN);
        headerRight->setJustifyContent(brls::JustifyContent::CENTER);
        headerRight->setAlignItems(brls::AlignItems::FLEX_END);
        headerRight->setGrow(1.0f);
        headerRight->setMarginRight(20);

        headerRight->addView(new HeaderStatusView());

        auto* attribution = new brls::Label();
        attribution->setText("akira/app/about"_i18n);
        attribution->setFontSize(14);
        attribution->setTextColor(active().textDim);
        attribution->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
        headerRight->addView(attribution);

        header->addView(headerRight);
    }

    auto* footer = appletFrame->getFooter();
    if (footer && !footer->getChildren().empty()) {
        if (auto* containerBox = dynamic_cast<brls::Box*>(footer->getChildren()[0])) {
            if (!containerBox->getChildren().empty()) {
                if (auto* rowBox = dynamic_cast<brls::Box*>(containerBox->getChildren()[0])) {
                    for (auto* child : rowBox->getChildren()) {
                        if (dynamic_cast<brls::Hints*>(child))
                            continue;
                        if (auto* box = dynamic_cast<brls::Box*>(child)) {
                            box->setVisibility(brls::Visibility::GONE);
                            break;
                        }
                    }
                }
            }
        }
    }
}

void AkiraHeader::openTrophies()
{
    /* Which screen asked, and how deep the stack was. Trophies opening from
     * under the controller picker could not be explained by borealis's
     * dispatch, and the focus at the moment of the call is what named the
     * cause: the home tab taking focus back from four activities down. */
    {
        auto* focus = brls::Application::getCurrentFocus();
        log::info("akiraOpenTrophies: focus={} stack={}",
                           focus ? focus->describe() : std::string("none"),
                           brls::Application::getActivitiesStack().size());
    }

    /*
     * Also guarded here, not only at registration: the action is registered once per view
     * but the active profile can change under it.
     */
    if (settings::SettingsManager::getInstance()->accounts().isLegacyProfileActive()) {
        brls::Application::notify("akira/trophies/no_psn_token"_i18n);
        return;
    }

    if (!settings::SettingsManager::getInstance()->accounts().activeProfileTrophiesEnabled()) {
        brls::Application::notify("akira/trophies/disabled_for_profile"_i18n);
        return;
    }
    auto* view = trophies::views::TrophyListTab::create();
    view->registerAction("akira/common/back"_i18n, brls::ControllerButton::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    }, false);
    auto* frame = new brls::AppletFrame(view);
    decorate(frame);
    brls::Application::pushActivity(new brls::Activity(frame));
}

void AkiraHeader::openSettings()
{
    if (settings::SettingsManager::getInstance()->accounts().profiles().empty()) {
        auto* setupFrame = new brls::AppletFrame(new accounts::views::ProfileSetupView(false, true));
        decorate(setupFrame);
        brls::Application::pushActivity(new brls::Activity(setupFrame));
        return;
    }
    auto* frame = new brls::AppletFrame(new settings::views::SettingsFrameView());
    decorate(frame);
    brls::Application::pushActivity(new brls::Activity(frame));
}

void AkiraHeader::registerTabActions(brls::View* view)
{
    /*
     * Trophies need a PSN token, so in legacy the action is not registered at all rather
     * than opening a screen that can only tell the user no.
     */
    if (!settings::SettingsManager::getInstance()->accounts().isLegacyProfileActive())
    {
        view->registerAction("akira/tabs/trophies"_i18n, brls::ControllerButton::BUTTON_LB, [](brls::View*) {
            openTrophies();
            return true;
        }, false);
    }
    view->registerAction("akira/tabs/settings"_i18n, brls::ControllerButton::BUTTON_RB, [](brls::View*) {
        openSettings();
        return true;
    }, false);
}

}
