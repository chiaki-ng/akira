#include "app/views/main_activity.hpp"

#include <arpa/inet.h>
#include <switch.h>

#include <borealis/core/i18n.hpp>
#include <borealis/views/hint.hpp>

#include "app/views/akira_header.hpp"
#include "app/views/theme.hpp"
#include "consoles/discovery_manager.hpp"
#include "consoles/views/host_list_tab.hpp"
#include "log/log.hpp"
#include "platform/factory.hpp"

using namespace brls::literals;

namespace akira::app::views {

std::string MainActivity::localIpAddress()
{
    u32 ip = 0;
    Result rc = nifmGetCurrentIpAddress(&ip);
    if (R_SUCCEEDED(rc) && ip != 0) {
        struct in_addr addr;
        addr.s_addr = ip;
        return std::string(inet_ntoa(addr));
    }
    return "Not connected";
}

void MainActivity::onResume()
{
    brls::Activity::onResume();
    consoles::views::HostListTab::refreshRailsIfActive();
}

void MainActivity::onContentAvailable()
{
    log::info("Main activity content available");

    auto* appletFrame = dynamic_cast<brls::AppletFrame*>(this->getContentView());
    if (appletFrame) {
        AkiraHeader::decorate(appletFrame);

        auto* footer = appletFrame->getFooter();
        if (footer) {
            auto& outerChildren = footer->getChildren();
            if (!outerChildren.empty()) {
                auto* containerBox = dynamic_cast<brls::Box*>(outerChildren[0]);
                if (containerBox && !containerBox->getChildren().empty()) {
                    if (auto* rowBox = dynamic_cast<brls::Box*>(containerBox->getChildren()[0]))
                        addFooterStatus(rowBox);
                }
            }
        }
    }

    this->registerAction("akira/tabs/trophies"_i18n, brls::ControllerButton::BUTTON_LB, [](brls::View*) {
        AkiraHeader::openTrophies();
        return true;
    }, false);

    this->registerAction("akira/tabs/settings"_i18n, brls::ControllerButton::BUTTON_RB, [](brls::View*) {
        AkiraHeader::openSettings();
        return true;
    }, false);

    platform::PlatformFactory::instance().updates().launched();
}

void MainActivity::addFooterStatus(brls::Box* rowBox)
{
    for (auto* child : rowBox->getChildren()) {
        if (dynamic_cast<brls::Hints*>(child))
            continue;
        if (auto* box = dynamic_cast<brls::Box*>(child)) {
            box->setVisibility(brls::Visibility::GONE);
            break;
        }
    }

    auto* footerCol = new brls::Box();
    footerCol->setAxis(brls::Axis::COLUMN);
    footerCol->setDirection(brls::Direction::LEFT_TO_RIGHT);
    footerCol->setJustifyContent(brls::JustifyContent::CENTER);
    footerCol->setAlignItems(brls::AlignItems::FLEX_START);

    auto* ipLabel = new brls::Label();
    ipLabel->setText("akira/app/ip_prefix"_i18n + localIpAddress());
    ipLabel->setFontSize(18);
    ipLabel->setTextColor(active().textDim);
    ipLabel->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    footerCol->addView(ipLabel);

    discoveryStatusLabel = new brls::Label();
    discoveryStatusLabel->setFontSize(13);
    discoveryStatusLabel->setTextColor(active().textDim);
    discoveryStatusLabel->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    footerCol->addView(discoveryStatusLabel);

    rowBox->addView(footerCol);

    discoveryStatusTimer.setCallback([this]() { showDiscoveryStatus(); });
    discoveryStatusTimer.start(500);
}

void MainActivity::showDiscoveryStatus()
{
    if (!discoveryStatusLabel)
        return;
    consoles::DiscoveryManager::SweepStatus st = consoles::DiscoveryManager::getInstance()->getSweepStatus();
    std::string text;
    if (!st.serviceRunning) {
        text = "akira/app/discovery_off"_i18n;
    } else if (!st.sweepActive || st.subnets.empty()) {
        text = "akira/app/discovery_local"_i18n;
    } else {
        std::string subs;
        for (size_t i = 0; i < st.subnets.size(); i++) {
            if (i > 0)
                subs += ", ";
            subs += st.subnets[i];
        }
        text = "akira/app/discovery_running_for"_i18n + " " + subs;
        if (!st.currentTarget.empty())
            text += " · " + st.currentTarget;
    }
    discoveryStatusLabel->setText(text);
}

}
