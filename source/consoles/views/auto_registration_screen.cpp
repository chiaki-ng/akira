#include "consoles/views/auto_registration_screen.hpp"
#include "app/view_services.hpp"

#include <borealis/core/i18n.hpp>

#include <string>

#include "consoles/console_actions.hpp"
#include "consoles/host.hpp"
#include "consoles/register_flow.hpp"
#include "consoles/views/host_list_tab.hpp"
#include "consoles/views/pin_registration_screen.hpp"

namespace akira::consoles::views {

using namespace brls::literals;

AutoRegistrationScreen::AutoRegistrationScreen(bool showStages)
    : showStages(showStages)
{
    if (!showStages)
        logPane.subscribe();
}

AutoRegistrationScreen::~AutoRegistrationScreen()
{
    logPane.unsubscribe();
}

void AutoRegistrationScreen::draw(NVGcontext* vg, float x, float y, float width, float height,
                                  brls::Style style, brls::FrameContext* ctx)
{
    Box::draw(vg, x, y, width, height, style, ctx);

    if (showStages)
        return;

    auto* logArea = this->getView("autoreg/logArea");
    if (!logArea)
        return;

    logPane.render(vg, logArea->getX(), logArea->getY(),
                   logArea->getWidth(), logArea->getHeight());
}

void AutoRegistrationScreen::showStage(int index, int total)
{
    if (!stageLabel)
        return;
    if (!app::ViewServices::current().appSettings.connectionShowStages())
        return;
    std::string name;
    switch (RegisterFlow::stage(index)) {
        case RegisterFlow::Stage::Account: name = "akira/hosts/register_stage_account"_i18n; break;
        case RegisterFlow::Stage::Console: name = "akira/hosts/register_stage_console"_i18n; break;
        case RegisterFlow::Stage::Connect: name = "akira/hosts/register_stage_connect"_i18n; break;
        case RegisterFlow::Stage::Finish: name = "akira/hosts/register_stage_finish"_i18n; break;
    }
    stageLabel->setText(brls::getStr("akira/hosts/register_stage_fmt", index, total, name));
}

void AutoRegistrationScreen::dismiss()
{
    stageLabel = nullptr;
    if (progress) {
        progress = nullptr;
        brls::Application::popActivity();
    }
}

void AutoRegistrationScreen::show(Host* host)
{
    bool showStages = app::ViewServices::current().appSettings.connectionShowStages();

    auto* box = new AutoRegistrationScreen(showStages);
    box->setAxis(brls::Axis::COLUMN);
    box->setJustifyContent(showStages ? brls::JustifyContent::CENTER
                                      : brls::JustifyContent::FLEX_START);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setWidthPercentage(100.0f);
    box->setHeightPercentage(100.0f);
    box->setBackgroundColor(nvgRGB(0, 0, 0));

    if (showStages) {
        auto* spinner = new brls::ProgressSpinner();
        spinner->setWidth(64);
        spinner->setHeight(64);
        box->addView(spinner);
    } else {
        box->setPadding(20, 40, 20, 40);
    }

    auto* label = new brls::Label();
    label->setText("akira/hosts/registering_psn"_i18n);
    label->setFontSize(showStages ? 22 : 24);
    if (showStages)
        label->setMarginTop(24);
    box->addView(label);
    stageLabel = label;

    if (!showStages) {
        auto* logArea = new brls::Box();
        logArea->setId("autoreg/logArea");
        logArea->setWidthPercentage(100.0f);
        logArea->setGrow(1.0f);
        box->addView(logArea);
    }

    auto* hint = new brls::Label();
    hint->setText("akira/hosts/register_cancel_hint"_i18n);
    hint->setFontSize(16);
    hint->setTextColor(nvgRGB(0x88, 0x88, 0x88));
    hint->setMarginTop(12);
    box->addView(hint);

    box->setFocusable(true);
    box->registerAction("akira/common/cancel"_i18n, brls::ControllerButton::BUTTON_B,
        [host](brls::View*) {
            RegisterFlow& flow = app::ViewServices::current().registration;
            flow.userCanceled();
            HostListTab::actions().cancelRegistration(*host);
            flow.stopped();
            dismiss();
            return true;
        }, false);

    auto* activity = new brls::Activity(box);
    progress = activity;
    brls::Application::pushActivity(activity);
    brls::Application::giveFocus(box);
}

void AutoRegistrationScreen::start(Host* host)
{
    RegisterFlow& flow = app::ViewServices::current().registration;
    if (!flow.beginInternet()) {
        return;
    }

    Host* hostPtr = host;

    show(host);
    showStage(1, 4);

    const HostRegisterError result = HostListTab::actions().registerOverInternet(*host,
        [](int index, int total) {
            showStage(index, total);
        },
        [hostPtr](Host::PairingOutcome outcome) {
            const RegisterFlow::InternetEnd end = app::ViewServices::current().registration.internetEnded(outcome);
            dismiss();
            switch (end) {
                case RegisterFlow::InternetEnd::Success:
                    brls::Application::notify("akira/hosts/registration_success"_i18n);
                    HostListTab::consoleChanged(hostPtr);
                    return;
                case RegisterFlow::InternetEnd::Quiet:
                    return;
                case RegisterFlow::InternetEnd::FallBackToPin:
                    break;
            }
            brls::Application::notify("akira/hosts/auto_register_fallback"_i18n);
            PinRegistrationScreen::open(hostPtr);
        });

    if (!flow.internetStarted(result)) {
        dismiss();
        brls::Application::notify("akira/hosts/auto_register_fallback"_i18n);
        PinRegistrationScreen::open(hostPtr);
    }
}

}
