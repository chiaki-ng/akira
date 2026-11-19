#include "settings/views/settings_controllers_view.hpp"
#include "app/view_services.hpp"
#include "settings/views/settings_pad_view.hpp"

#include "app/view_services.hpp"
#include "platform/factory.hpp"

#include <borealis/core/i18n.hpp>

namespace akira::settings::views {

using namespace brls::literals;

std::string SettingsControllersView::tierSummary(const akira::platform::PadDescription& pad)
{
    auto* services = &app::ViewServices::current();
    const auto padSettings = app::ViewServices::current().platform.createPadSettings(pad);

    if (auto mode = padSettings->outputMode(services->controllers))
        return *mode == play::PadOutputMode::Basic
                   ? "akira/settings/rumble_output_basic"_i18n
                   : "akira/settings/rumble_output_native"_i18n;

    const std::string key = padSettings->currentKey(services->controllers);

    if (key == play::kRumbleKeySwitch)
        return "akira/settings/rumble_tier_switch"_i18n;
    if (key == play::kRumbleKeyDefault)
        return "akira/settings/rumble_tier_default"_i18n;
    if (padSettings->isUnitKey(key))
        return "akira/settings/rumble_tier_unit"_i18n;

    return key;
}

SettingsControllersView::SettingsControllersView()
{
    setAxis(brls::Axis::COLUMN);
    setWidth(brls::View::AUTO);
    setHeight(brls::View::AUTO);
    setGrow(1.0f);
    setPadding(30.0f, 40.0f, 30.0f, 40.0f);
    setBackgroundColor(brls::Application::getTheme()["brls/background"]);

    auto* title = new brls::Label();
    title->setText("akira/settings/connected_controllers"_i18n);
    title->setFontSize(30.0f);
    addView(title);

    auto* hint = new brls::Label();
    hint->setText("akira/settings/connected_controllers_hint"_i18n);
    hint->setFontSize(16.0f);
    hint->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    hint->setMarginBottom(20.0f);
    addView(hint);

    auto* scroll = new brls::ScrollingFrame();
    scroll->setWidth(brls::View::AUTO);
    scroll->setHeight(brls::View::AUTO);
    scroll->setGrow(1.0f);

    list = new brls::Box(brls::Axis::COLUMN);
    list->setWidth(brls::View::AUTO);
    list->setHeight(brls::View::AUTO);
    list->setCornerRadius(14.0f);
    list->setBackgroundColor(brls::Application::getTheme()["color/card"]);
    scroll->setContentView(list);
    addView(scroll);

    rebuild();

    this->registerAction("akira/common/back"_i18n, brls::ControllerButton::BUTTON_B,
                         [](brls::View*) {
                             brls::Application::popActivity();
                             return true;
                         }, true);
}

void SettingsControllersView::rebuild()
{
    list->clearViews();

    auto* services = &app::ViewServices::current();
    const std::vector<akira::platform::PadDescription> pads = app::ViewServices::current().platform.pads().scanPads();

    if (pads.empty()) {
        auto* empty = new brls::Label();
        empty->setText("akira/settings/connected_controllers_none"_i18n);
        empty->setFontSize(16.0f);
        empty->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        empty->setMargins(30.0f, 22.0f, 30.0f, 22.0f);
        list->addView(empty);
        return;
    }

    for (const akira::platform::PadDescription& pad : pads) {
        services->controllers.seedRumbleProfile(
            app::ViewServices::current().platform.createPadSettings(pad)->rumbleKey(services->controllers));

        auto* cell = new brls::DetailCell();
        cell->setMarginLeft(15.0f);
        cell->setMarginRight(15.0f);
        cell->setText(pad.label);
        cell->setDetailText(tierSummary(pad));

        cell->registerClickAction([pad](brls::View*) {
            brls::Application::pushActivity(new brls::Activity(new SettingsPadView(pad)),
                                            brls::TransitionAnimation::NONE);
            return true;
        });

        list->addView(cell);
    }

    services->store.save();
}

}
