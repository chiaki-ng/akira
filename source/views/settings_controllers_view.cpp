#include "views/settings_controllers_view.hpp"
#include "views/settings_pad_view.hpp"

#include "core/settings_manager.hpp"
#if AKIRA_PLATFORM_NX
#include "input/extended_input_manager.hpp"
#endif
#include "input/pad_path.hpp"
#if AKIRA_PLATFORM_NX
#include "input/ps_output.hpp"
#endif

#include <borealis/core/i18n.hpp>

using namespace brls::literals;

std::string SettingsControllersView::tierSummary(const akira::input::PadDescription& pad)
{
#if AKIRA_PLATFORM_NX
    const bool switchNative = pad.kind == akira::input::PadPathKind::JoyCon
                           || pad.kind == akira::input::PadPathKind::SwitchPro
                           ;

    auto* settings = SettingsManager::getInstance();

    if (akira::input::PadTakesDirectOutput(pad.vendor_id, pad.product_id)) {
        const akira::input::RumbleProfile profile = settings->resolveRumbleProfile(
            pad.vendor_id, pad.product_id,
            pad.has_address ? pad.bt_addr : nullptr, switchNative,
            pad.kind == akira::input::PadPathKind::JoyCon);

        return profile.output_mode == akira::input::PadOutputMode::Basic
                   ? "akira/settings/rumble_output_basic"_i18n
                   : "akira/settings/rumble_output_native"_i18n;
    }

    const std::string key = settings->resolveRumbleKey(
        pad.vendor_id, pad.product_id,
        pad.has_address ? pad.bt_addr : nullptr, switchNative,
        pad.kind == akira::input::PadPathKind::JoyCon);
#else
    const std::string key = SettingsManager::getInstance()->resolveRumbleKey(
        pad.kind == akira::input::PadPathKind::JoyCon);
#endif

    if (key == akira::input::kRumbleKeySwitch)
        return "akira/settings/rumble_tier_switch"_i18n;
    if (key == akira::input::kRumbleKeyDefault)
        return "akira/settings/rumble_tier_default"_i18n;
#if AKIRA_PLATFORM_NX
    if (akira::input::RumbleKeyIsUnit(key))
        return "akira/settings/rumble_tier_unit"_i18n;
#endif

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

    auto* settings = SettingsManager::getInstance();
    const std::vector<akira::input::PadDescription> pads = akira::input::DescribePads();

    if (pads.empty()) {
        auto* empty = new brls::Label();
        empty->setText("akira/settings/connected_controllers_none"_i18n);
        empty->setFontSize(16.0f);
        empty->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        empty->setMargins(30.0f, 22.0f, 30.0f, 22.0f);
        list->addView(empty);
        return;
    }

    for (const akira::input::PadDescription& pad : pads) {
#if AKIRA_PLATFORM_NX
        const bool switchNative = pad.kind == akira::input::PadPathKind::JoyCon
                               || pad.kind == akira::input::PadPathKind::SwitchPro
                               ;

        if (!switchNative && (pad.vendor_id != 0 || pad.product_id != 0)) {
            const akira::input::RumbleProfile current = settings->resolveRumbleProfile(
                pad.vendor_id, pad.product_id,
                pad.has_address ? pad.bt_addr : nullptr, switchNative,
                pad.kind == akira::input::PadPathKind::JoyCon);
            settings->seedRumbleProfile(
                akira::input::RumbleKeyForModel(pad.vendor_id, pad.product_id), &current);
        } else
            settings->seedRumbleProfile(settings->resolveRumbleKey(
                pad.vendor_id, pad.product_id,
                pad.has_address ? pad.bt_addr : nullptr, switchNative,
                pad.kind == akira::input::PadPathKind::JoyCon));
#else
        settings->seedRumbleProfile(settings->resolveRumbleKey(
            pad.kind == akira::input::PadPathKind::JoyCon));
#endif

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

    settings->writeFile();
}
