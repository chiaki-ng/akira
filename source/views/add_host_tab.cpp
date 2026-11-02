#include "views/add_host_tab.hpp"
#include "ui/theme.hpp"
#include "core/host.hpp"

#include <borealis/core/i18n.hpp>
using namespace brls::literals;

AddHostTab::AddHostTab() {
    this->inflateFromXMLRes("xml/tabs/add_host.xml");

    settings = SettingsManager::getInstance();

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
        statusLabel->setTextColor(akira::ui::active().danger);
        return;
    }

    if (addr.empty()) {
        statusLabel->setText("akira/add_host/error_no_ip"_i18n);
        statusLabel->setTextColor(akira::ui::active().danger);
        return;
    }

    if (!SettingsManager::isValidHostAddress(addr)) {
        statusLabel->setText("akira/add_host/error_invalid_ip"_i18n);
        statusLabel->setTextColor(akira::ui::active().danger);
        return;
    }

    auto* hostsMap = settings->getHostsMap();
    if (hostsMap->find(name) != hostsMap->end()) {
        statusLabel->setText("akira/add_host/error_duplicate_name"_i18n);
        statusLabel->setTextColor(akira::ui::active().danger);
        return;
    }

    Host* host = settings->getOrCreateHost(name);
    host->setHostType(HostType::Manual);
    settings->setHostAddr(host, addr);
    settings->setDiscovered(host, true);

    host->setFirmware(Host::Firmware::newest(targetIndex == 0 ? libchiaking::consoles::Generation::ps4
                                                              : libchiaking::consoles::Generation::ps5));

    settings->writeFile();

    statusLabel->setText("akira/add_host/success"_i18n);
    statusLabel->setTextColor(akira::ui::active().success);

    brls::Logger::info("Added host: {} at {}", name, addr);

    hostNameInput->setValue("");
    hostAddrInput->setValue("");
}
