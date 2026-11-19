#include "consoles/views/add_host_tab.hpp"
#include "app/view_services.hpp"
#include "app/views/theme.hpp"
#include "consoles/console_actions.hpp"
#include "play/chiaki_core.hpp"

#include <borealis/core/i18n.hpp>

namespace akira::consoles::views {

using namespace brls::literals;

AddHostTab::AddHostTab() {
    this->inflateFromXMLRes("xml/tabs/add_host.xml");

    services = &app::ViewServices::current();

    hostNameInput->init(
        "akira/add_host/console_name"_i18n,
        "",
        [](std::string text) {},
        "akira/add_host/console_name_placeholder"_i18n,
        "akira/add_host/console_name_hint"_i18n
    );

    hostAddrInput->init(
        "akira/add_host/ip_address"_i18n,
        "",
        [](std::string text) {},
        "akira/add_host/ip_placeholder"_i18n,
        "akira/add_host/ip_hint"_i18n
    );

    std::vector<std::string> targetOptions = {"PS4", "PS5"};
    targetSelector->init(
        "akira/add_host/console_type"_i18n,
        targetOptions,
        1,  // Default to PS5
        [](int selected) {},
        [](int selected) {}
    );

    saveBtn->registerClickAction([this](brls::View* view) {
        onSaveClicked();
        return true;
    });
}

brls::View* AddHostTab::create() {
    return new AddHostTab();
}

void AddHostTab::onSaveClicked() {
    std::string name = hostNameInput->getValue();
    std::string addr = hostAddrInput->getValue();
    int targetIndex = targetSelector->getSelection();

    ConsoleActions actions(services->hosts, services->store, services->accounts, services->developer,
                           services->wireGuard, play::ChiakiCore::current());
    const auto added = actions.addManual(name, addr,
        targetIndex == 0 ? libchiaking::consoles::Generation::ps4 : libchiaking::consoles::Generation::ps5);
    if (!added) {
        switch (added.error()) {
            case ConsoleActions::ManualRefusal::NoName:
                statusLabel->setText("akira/add_host/error_no_name"_i18n);
                break;
            case ConsoleActions::ManualRefusal::NoAddress:
                statusLabel->setText("akira/add_host/error_no_ip"_i18n);
                break;
            case ConsoleActions::ManualRefusal::InvalidAddress:
                statusLabel->setText("akira/add_host/error_invalid_ip"_i18n);
                break;
            case ConsoleActions::ManualRefusal::DuplicateName:
                statusLabel->setText("akira/add_host/error_duplicate_name"_i18n);
                break;
        }
        statusLabel->setTextColor(app::views::active().danger);
        return;
    }

    statusLabel->setText("akira/add_host/success"_i18n);
    statusLabel->setTextColor(app::views::active().success);

    hostNameInput->setValue("");
    hostAddrInput->setValue("");
}

}
