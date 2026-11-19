#include "settings/views/settings_frame_view.hpp"
#include "app/views/theme.hpp"
#include "app/views/motion.hpp"
#include "settings/views/settings_general_view.hpp"
#include "settings/views/settings_picture_view.hpp"
#include "settings/views/settings_controller_view.hpp"
#include "settings/views/settings_debug_view.hpp"
#include "accounts/views/settings_account_view.hpp"
#include "settings/views/settings_poweruser_view.hpp"
#include "settings/views/settings_developer_view.hpp"
#include "platform/factory.hpp"
#include "app/views/build_info_tab.hpp"
#include "settings/views/config_view_tab.hpp"
#include "network/views/network_utilities_tab.hpp"
#include "settings/settings_manager.hpp"
#include "settings/settings_descriptions.hpp"

#include <borealis/core/i18n.hpp>

namespace akira::settings::views {

using namespace brls::literals;

SettingsFrameView* SettingsFrameView::currentInstance = nullptr;

SettingsFrameView::SettingsFrameView() {
    currentInstance = this;
    buildMenus();

    this->setAxis(brls::Axis::COLUMN);
    this->setWidthPercentage(100.0f);
    this->setHeightPercentage(100.0f);
    this->setBackgroundColor(brls::Application::getTheme()["brls/background"]);

    menuBar = new brls::Box();
    menuBar->setAxis(brls::Axis::ROW);
    menuBar->setPadding(18.0f, 40.0f, 12.0f, 40.0f);
    this->addView(menuBar);

    subBar = new brls::Box();
    subBar->setAxis(brls::Axis::ROW);
    subBar->setPaddingLeft(40.0f);
    subBar->setPaddingRight(40.0f);
    subBar->setPaddingBottom(10.0f);
    this->addView(subBar);

    auto* split = new brls::Box();
    split->setAxis(brls::Axis::ROW);
    split->setGrow(1.0f);
    split->setWidthPercentage(100.0f);

    contentHolder = new brls::Box();
    contentHolder->setAxis(brls::Axis::COLUMN);
    contentHolder->setWidthPercentage(70.0f);
    contentHolder->setHeightPercentage(100.0f);
    split->addView(contentHolder);

    descPanel = new brls::Box();
    descPanel->setAxis(brls::Axis::COLUMN);
    descPanel->setWidthPercentage(30.0f);
    descPanel->setHeightPercentage(100.0f);
    descPanel->setPadding(34.0f, 34.0f, 34.0f, 34.0f);
    descPanel->setBackgroundColor(brls::Application::getTheme()["color/card"]);

    descTitle = new brls::Label();
    descTitle->setFontSize(20.0f);
    descTitle->setMarginBottom(12.0f);
    descTitle->setWidthPercentage(100.0f);
    descPanel->addView(descTitle);

    descBody = new brls::Label();
    descBody->setFontSize(15.0f);
    descBody->setTextColor(app::views::active().textMuted);
    descBody->setWidthPercentage(100.0f);
    descPanel->addView(descBody);

    descImage = new brls::Image();
    descImage->setWidthPercentage(100.0f);
    descImage->setMarginTop(18.0f);
    descImage->setVisibility(brls::Visibility::GONE);
    descPanel->addView(descImage);

    split->addView(descPanel);
    this->addView(split);

    renderMenuBar();
    renderSubBar();
    loadContent(false);

    focusSub = brls::Application::getGlobalFocusChangeEvent()->subscribe([this](brls::View*) {
        this->updateDescriptionFromFocus();
    });

    this->registerAction("akira/settings/menu"_i18n, brls::ControllerButton::BUTTON_LB, [this](brls::View*) { switchMenu(-1); return true; }, false);
    this->registerAction("akira/settings/menu"_i18n, brls::ControllerButton::BUTTON_RB, [this](brls::View*) { switchMenu(1); return true; }, false);
    this->registerAction("", brls::ControllerButton::BUTTON_LT, [this](brls::View*) { switchSub(-1); return true; }, true);
    this->registerAction("", brls::ControllerButton::BUTTON_RT, [this](brls::View*) { switchSub(1); return true; }, true);
    this->registerAction("akira/common/back"_i18n, brls::ControllerButton::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    }, false);
}

SettingsFrameView::~SettingsFrameView() {
    *alive = false;
    if (currentInstance == this)
        currentInstance = nullptr;
    brls::Application::getGlobalFocusChangeEvent()->unsubscribe(focusSub);
}

void SettingsFrameView::onActiveProfileChanged() {
    /*
     * The profile switcher lives inside the Account tab, so a legacy <-> normal switch
     * happens while this frame is on screen and its menu bar was built for the other
     * mode. Deferred, because rebuilding here would free the switcher whose click
     * handler is still running.
     */
    auto guard = alive;
    brls::sync([this, guard]() {
        if (!*guard)
            return;
        buildMenus();
        activeMenu = 0;
        activeSub = 0;
        renderMenuBar();
        renderSubBar();
        loadContent(false);
    });
}

void SettingsFrameView::buildMenus() {
    menus.clear();

    menus.push_back({"akira/settings/menu_account"_i18n, {{"", []() -> brls::Box* { return new accounts::views::SettingsAccountView(); }}}});
    menus.push_back({"akira/settings/menu_general"_i18n, {{"", []() -> brls::Box* { return new SettingsGeneralView(); }}}});
    for (auto& menu : akira::platform::PlatformFactory::instance().updates().settingsMenus())
        menus.push_back({std::move(menu.title), {{"", std::move(menu.create)}}});
    menus.push_back({"akira/settings/menu_quality"_i18n, {{"", []() -> brls::Box* { return new SettingsPictureView(); }}}});
    menus.push_back({"akira/settings/menu_controls"_i18n, {{"", []() -> brls::Box* { return new SettingsControllerView(); }}}});
    menus.push_back({"akira/settings/menu_debug"_i18n, {{"", []() -> brls::Box* { return new SettingsDebugView(); }}}});
    menus.push_back({"akira/settings/menu_build_info"_i18n, {{"", []() -> brls::Box* { return new app::views::BuildInfoTab(); }}}});
    menus.push_back({"akira/settings/menu_config"_i18n, {{"", []() -> brls::Box* { return new ConfigViewTab(); }}}});
    menus.push_back({"akira/settings/menu_network"_i18n, {{"", []() -> brls::Box* { return new network::views::NetworkUtilitiesTab(); }}}});

    if (SettingsManager::getInstance()->developer().powerUserMenuUnlocked()) {
        menus.push_back({"akira/settings/menu_power_user"_i18n, {{"", []() -> brls::Box* { return new SettingsPowerUserView(); }}}});
        menus.push_back({"akira/settings/menu_developer"_i18n, {{"", []() -> brls::Box* { return new SettingsDeveloperView(); }}}});
    }
}

void SettingsFrameView::renderMenuBar() {
    menuBar->clearViews();
    NVGcolor accent = app::views::active().accent;
    for (size_t i = 0; i < menus.size(); i++) {
        auto* lbl = new brls::Label();
        lbl->setText(menus[i].label);
        lbl->setFontSize(18.0f);
        lbl->setMarginRight(28.0f);
        lbl->setTextColor(static_cast<int>(i) == activeMenu ? accent : app::views::active().textDim);
        menuBar->addView(lbl);
    }
}

void SettingsFrameView::renderSubBar() {
    subBar->clearViews();
    auto& subs = menus[activeMenu].subs;
    bool hasNamed = subs.size() > 1;
    subBar->setVisibility(hasNamed ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (!hasNamed)
        return;

    NVGcolor accent = app::views::active().accent;
    for (size_t i = 0; i < subs.size(); i++) {
        auto* lbl = new brls::Label();
        lbl->setText(subs[i].name);
        lbl->setFontSize(15.0f);
        lbl->setMarginRight(22.0f);
        lbl->setTextColor(static_cast<int>(i) == activeSub ? accent : app::views::active().textDim);
        subBar->addView(lbl);
    }
}

void SettingsFrameView::loadContent(bool focus) {
    contentHolder->clearViews();
    auto* v = menus[activeMenu].subs[activeSub].make();
    contentHolder->addView(v);
    if (focus) {
        brls::View* def = v->getDefaultFocus();
        if (def)
            brls::Application::giveFocus(def);
    }
}

void SettingsFrameView::switchMenu(int delta) {
    int n = static_cast<int>(menus.size());
    activeMenu = ((activeMenu + delta) % n + n) % n;
    activeSub = 0;
    renderMenuBar();
    renderSubBar();
    app::views::motion::crossfade(contentHolder, contentFade, [this]() { loadContent(true); });
}

void SettingsFrameView::switchSub(int delta) {
    int s = static_cast<int>(menus[activeMenu].subs.size());
    if (s <= 1)
        return;
    activeSub = ((activeSub + delta) % s + s) % s;
    renderSubBar();
    app::views::motion::crossfade(contentHolder, contentFade, [this]() { loadContent(true); });
}

void SettingsFrameView::updateDescriptionFromFocus() {
    brls::View* f = brls::Application::getCurrentFocus();
    while (f) {
        SettingDescription d;
        if (settings::lookupSettingDescription(f->getId(), d)) {
            if (!d.title.empty() || !d.body.empty()) {
                descTitle->setText(d.title);
                descBody->setText(d.body);
                if (!d.image.empty()) {
                    descImage->setImageFromRes(d.image);
                    descImage->setVisibility(brls::Visibility::VISIBLE);
                } else {
                    descImage->setVisibility(brls::Visibility::GONE);
                }
                showDescPanel(true);
                return;
            }
            break;
        }
        f = f->getParent();
    }
    showDescPanel(false);
}

void SettingsFrameView::showDescPanel(bool show) {
    if (!descPanel)
        return;
    descPanel->setVisibility(show ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (contentHolder)
        contentHolder->setWidthPercentage(show ? 70.0f : 100.0f);
}

}
