#include "settings/views/settings_controller_view.hpp"
#include "play/views/controller_remap_view.hpp"
#include "platform/factory.hpp"
#include "settings/views/settings_controllers_view.hpp"

#include <borealis/core/i18n.hpp>
#include <algorithm>

namespace akira::settings::views {

using namespace brls::literals;

SettingsControllerView::SettingsControllerView() {
    this->inflateFromXMLRes("xml/settings/controller.xml");

    settings = SettingsManager::getInstance();

    initSleepOnExitToggle();
    initConnectedControllers();
    initButtonMappingCell();

}

void SettingsControllerView::initSleepOnExitToggle() {
    bool currentValue = settings->app().sleepOnExit();

    sleepOnExitToggle->init(
        "akira/settings/sleep_on_exit"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->app().setSleepOnExit(isOn);
            settings->store().save();
        }
    );
}

void SettingsControllerView::initConnectedControllers()
{
    connectedControllersCell->setText("akira/settings/connected_controllers"_i18n);

    const size_t count = akira::platform::PlatformFactory::instance().pads().scanPads().size();
    connectedControllersCell->setDetailText(
        count == 0 ? "akira/settings/connected_controllers_none"_i18n
                   : std::to_string(count));

    connectedControllersCell->registerClickAction([](brls::View*) {
        brls::Application::pushActivity(new brls::Activity(new SettingsControllersView()),
                                        brls::TransitionAnimation::NONE);
        return true;
    });
}

void SettingsControllerView::initButtonMappingCell() {
    buttonMappingCell->setText("akira/settings/button_mapping"_i18n);
    buttonMappingCell->setDetailText("akira/common/configure"_i18n);

    buttonMappingCell->registerClickAction([](brls::View*) {
        auto* remapView = new play::views::ControllerRemapView();
        brls::Application::pushActivity(new brls::Activity(remapView), brls::TransitionAnimation::NONE);
        return true;
    });
}

}
