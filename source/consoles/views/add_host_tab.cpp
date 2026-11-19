#include "consoles/views/add_host_tab.hpp"
#include "app/view_services.hpp"
#include "app/views/theme.hpp"
#include "consoles/console_address.hpp"
#include "consoles/host.hpp"
#include "log/log.hpp"

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

    if (name.empty()) {
        statusLabel->setText("akira/add_host/error_no_name"_i18n);
        statusLabel->setTextColor(app::views::active().danger);
        return;
    }

    if (addr.empty()) {
        statusLabel->setText("akira/add_host/error_no_ip"_i18n);
        statusLabel->setTextColor(app::views::active().danger);
        return;
    }

    if (!ConsoleAddress::parse(addr)) {
        statusLabel->setText("akira/add_host/error_invalid_ip"_i18n);
        statusLabel->setTextColor(app::views::active().danger);
        return;
    }

    auto* hostsMap = &services->hosts.entries();
    if (hostsMap->find(name) != hostsMap->end()) {
        statusLabel->setText("akira/add_host/error_duplicate_name"_i18n);
        statusLabel->setTextColor(app::views::active().danger);
        return;
    }

    Host* host = services->hosts.getOrCreate(name);
    host->setHostType(HostType::Manual);
    host->setHostAddr(addr);
    host->setDiscovered(true);

    host->setFirmware(Host::Firmware::newest(targetIndex == 0 ? libchiaking::consoles::Generation::ps4
                                                              : libchiaking::consoles::Generation::ps5));

    services->store.save();

    statusLabel->setText("akira/add_host/success"_i18n);
    statusLabel->setTextColor(app::views::active().success);

    log::info("Added host: {} at {}", name, addr);

    hostNameInput->setValue("");
    hostAddrInput->setValue("");
}

}
