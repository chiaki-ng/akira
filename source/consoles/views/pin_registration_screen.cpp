#include "consoles/views/pin_registration_screen.hpp"
#include "app/view_services.hpp"
#include "log/log.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>

#include "consoles/console_actions.hpp"
#include "consoles/host.hpp"
#include "consoles/views/enter_pin_view.hpp"
#include "consoles/views/host_list_tab.hpp"

namespace akira::consoles::views {

using namespace brls::literals;

void PinRegistrationScreen::open(Host* host)
{
    if (app::ViewServices::current().registration.registering()) {
        return;
    }

    Host* hostPtr = host;

    auto* pinView = new EnterPinView(host, PinViewType::Registration);
    pinView->setOnCancel([hostPtr]() {
        HostListTab::actions().cancelRegistration(*hostPtr);
        app::ViewServices::current().registration.stopped();
        log::info("PIN entry cancelled");
    });
    pinView->setOnPinEntered([hostPtr](const std::string& pin) {
        RegisterFlow& flow = app::ViewServices::current().registration;
        flow.beginPin();
        const HostRegisterError result = HostListTab::actions().registerWithPin(*hostPtr, pin, [hostPtr](Host::PairingOutcome outcome) {
            switch (app::ViewServices::current().registration.pinEnded(outcome)) {
                case RegisterFlow::Notice::Success:
                    brls::Application::notify("akira/hosts/registration_success"_i18n);
                    HostListTab::consoleChanged(hostPtr);
                    break;
                case RegisterFlow::Notice::Canceled:
                    brls::Application::notify("akira/hosts/registration_canceled"_i18n);
                    break;
                case RegisterFlow::Notice::Failed:
                    brls::Application::notify("akira/hosts/registration_failed"_i18n);
                    break;
            }
        });
        if (const auto refusal = flow.pinRefused(result))
            brls::Application::notify(refusalText(*refusal));
    });

    brls::Application::pushActivity(new brls::Activity(pinView));
}

std::string PinRegistrationScreen::refusalText(RegisterFlow::Refusal refusal)
{
    switch (refusal) {
        case RegisterFlow::Refusal::AccountId: return "akira/hosts/error_psn_account_id"_i18n;
        case RegisterFlow::Refusal::OnlineId: return "akira/hosts/error_psn_online_id"_i18n;
        case RegisterFlow::Refusal::ConsoleType: return "akira/hosts/error_console_type"_i18n;
        case RegisterFlow::Refusal::Failed: break;
    }
    return "akira/hosts/error_registration_failed"_i18n;
}

}
