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

	std::vector<std::string> targetOptions = {
    "PlayStation 4 Fat", 
    "PlayStation 4 Slim", 
    "PlayStation 4 Pro", 
    "PlayStation 5 Fat", 
    "PlayStation 5 Slim", 
    "PlayStation 5 Pro"
	};
    targetSelector->init(
        "akira/add_host/console_type"_i18n,
        targetOptions,
        3,  // Default to PS5
        [](int selected) {},
        [](int selected) {}
    );

    auto* saveBtn = this->getView("add/saveBtn");
    if (saveBtn) {
        auto* label = dynamic_cast<brls::Label*>(saveBtn->getView("add/saveBtnLabel"));
        if (label) {
            label->setText("akira/add_host/add_console"_i18n);
            label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        }
        saveBtn->registerClickAction([this](brls::View* view) {
            onSaveClicked();
            return true;
        });
    };
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

	ChiakiTarget target;
    ConsoleModel model;
    
    switch (targetIndex) {
        case 0:
            target = CHIAKI_TARGET_PS4_10;
            model = ConsoleModel::PS4Fat;
            break;
        case 1:
            target = CHIAKI_TARGET_PS4_10;
            model = ConsoleModel::PS4Slim;
            break;
        case 2:
            target = CHIAKI_TARGET_PS4_10;
            model = ConsoleModel::PS4Pro;
            break;
        case 3:
            target = CHIAKI_TARGET_PS5_1;
            model = ConsoleModel::PS5Fat;
            break;
        case 4:
            target = CHIAKI_TARGET_PS5_1;
            model = ConsoleModel::PS5Slim;
            break;
        case 5:
        default:
            target = CHIAKI_TARGET_PS5_1;
            model = ConsoleModel::PS5Pro;
            break;
    }
    
    settings->setChiakiTarget(host, target);
    host->setConsoleModel(model);

    settings->writeFile();

    statusLabel->setText("akira/add_host/success"_i18n);
    statusLabel->setTextColor(akira::ui::active().success);

    brls::Logger::info("Added host: {} at {}", name, addr);

    hostNameInput->setValue("");
    hostAddrInput->setValue("");
}
