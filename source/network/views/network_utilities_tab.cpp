#include "network/views/network_utilities_tab.hpp"
#include "app/ui_dispatcher.hpp"
#include "app/view_services.hpp"
#include "app/views/theme.hpp"
#include "consoles/discovery_manager.hpp"
#include "network/nat_probe.hpp"
#include "network/wireguard_manager.hpp"
#include "play/chiaki_core.hpp"
#include <borealis/core/i18n.hpp>

#include <utility>

namespace akira::network::views {

using namespace brls::literals;

NetworkUtilitiesTab::NetworkUtilitiesTab() {
    this->inflateFromXMLRes("xml/tabs/network_utilities_tab.xml");

    checkNatBtn->registerClickAction([this](brls::View* view) {
        onCheckNatClicked();
        return true;
    });

    initWireGuardUI();
}

NetworkUtilitiesTab::~NetworkUtilitiesTab() {
    *alive = false;
}

void NetworkUtilitiesTab::initWireGuardUI() {
    auto& wg = app::ViewServices::current().wireGuard;

    if (!wg.configExists()) {
        auto* noConfigLabel = new brls::Label();
        noConfigLabel->setText("akira/network/wg_no_config"_i18n);
        noConfigLabel->setFontSize(18);
        noConfigLabel->setTextColor(app::views::active().warning);
        wgStatusContainer->addView(noConfigLabel);

        auto* pathLabel = new brls::Label();
        pathLabel->setText("akira/network/wg_config_path"_i18n);
        pathLabel->setFontSize(14);
        pathLabel->setTextColor(app::views::active().textDim);
        wgStatusContainer->addView(pathLabel);
        return;
    }

    wgConnectBtn = new brls::Button();
    wgConnectBtn->setText("akira/network/wg_connect"_i18n);
    wgConnectBtn->setStyle(&brls::BUTTONSTYLE_PRIMARY);
    wgConnectBtn->setMarginRight(15);
    wgConnectBtn->registerClickAction([this](brls::View* view) {
        onWgConnectClicked();
        return true;
    });
    wgButtonContainer->addView(wgConnectBtn);

    wgDisconnectBtn = new brls::Button();
    wgDisconnectBtn->setText("akira/network/wg_disconnect"_i18n);
    wgDisconnectBtn->setStyle(&brls::BUTTONSTYLE_BORDERED);
    wgDisconnectBtn->registerClickAction([this](brls::View* view) {
        onWgDisconnectClicked();
        return true;
    });
    wgButtonContainer->addView(wgDisconnectBtn);

    updateWireGuardStatus();
}

void NetworkUtilitiesTab::updateWireGuardStatus() {
    auto& wg = app::ViewServices::current().wireGuard;

    wgStatusContainer->clearViews();

    auto* statusRow = new brls::Box();
    statusRow->setAxis(brls::Axis::ROW);
    statusRow->setMarginBottom(10);

    auto* statusLabel = new brls::Label();
    statusLabel->setText("akira/network/wg_status"_i18n);
    statusLabel->setFontSize(18);
    statusLabel->setTextColor(app::views::active().textDim);
    statusRow->addView(statusLabel);

    auto* statusValue = new brls::Label();
    if (isWgConnecting) {
        statusValue->setText("akira/network/wg_connecting"_i18n);
        statusValue->setTextColor(app::views::active().warning);
    } else if (wg.isConnected()) {
        statusValue->setText("akira/network/wg_connected"_i18n);
        statusValue->setTextColor(app::views::active().success);
    } else {
        statusValue->setText("akira/network/wg_disconnected"_i18n);
        statusValue->setTextColor(app::views::active().danger);
    }
    statusValue->setFontSize(18);
    statusRow->addView(statusValue);

    wgStatusContainer->addView(statusRow);

    if (wg.isConnected()) {
        auto* ipRow = new brls::Box();
        ipRow->setAxis(brls::Axis::ROW);

        auto* ipLabel = new brls::Label();
        ipLabel->setText("akira/network/wg_tunnel_ip"_i18n);
        ipLabel->setFontSize(16);
        ipLabel->setTextColor(app::views::active().textDim);
        ipRow->addView(ipLabel);

        auto* ipValue = new brls::Label();
        ipValue->setText(wg.getTunnelIP());
        ipValue->setFontSize(16);
        ipValue->setTextColor(app::views::active().textMuted);
        ipRow->addView(ipValue);

        wgStatusContainer->addView(ipRow);
    }

    if (!wg.getLastError().empty() && !wg.isConnected()) {
        auto* errorLabel = new brls::Label();
        errorLabel->setText(brls::getStr("akira/network/wg_error", wg.getLastError()));
        errorLabel->setFontSize(14);
        errorLabel->setTextColor(app::views::active().danger);
        wgStatusContainer->addView(errorLabel);
    }

    if (wgConnectBtn && wgDisconnectBtn) {
        wgConnectBtn->setVisibility(wg.isConnected() || isWgConnecting ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
        wgDisconnectBtn->setVisibility(wg.isConnected() ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    }
}

void NetworkUtilitiesTab::onWgConnectClicked() {
    if (isWgConnecting)
        return;

    isWgConnecting = true;
    updateWireGuardStatus();

    auto guard = alive;
    brls::async([this, guard]() {
        bool success = app::ViewServices::current().wireGuard.connect();
        brls::sync([this, guard, success]() {
            if (!*guard)
                return;
            isWgConnecting = false;
            if (success)
                app::ViewServices::current().discovery.restartSearch();
            updateWireGuardStatus();
        });
    });
}

void NetworkUtilitiesTab::onWgDisconnectClicked() {
    app::ViewServices::current().wireGuard.disconnect();
    app::ViewServices::current().discovery.restartSearch();
    updateWireGuardStatus();
}

brls::View* NetworkUtilitiesTab::create() {
    return new NetworkUtilitiesTab();
}

void NetworkUtilitiesTab::onCheckNatClicked() {
    if (isChecking) {
        return;
    }

    isChecking = true;
    clearResults();
    addResultRow("", "akira/network/testing"_i18n, app::views::active().textDim);

    auto& services = app::ViewServices::current();
    services.dispatcher.runOnWorker([this, guard = alive, &dispatcher = services.dispatcher, &core = services.core]() {
        NatReport report(NatProbe(core).check());
        dispatcher.post([this, guard, report = std::move(report)]() {
            if (!*guard)
                return;
            isChecking = false;
            displayResult(report);
        });
    });
}

void NetworkUtilitiesTab::clearResults() {
    resultContainer->clearViews();
}

void NetworkUtilitiesTab::displayResult(const NatReport& report) {
    clearResults();

    if (!report.found()) {
        addResultRow("akira/common/error"_i18n, brls::getStr(report.detailKey()), app::views::active().danger);
        return;
    }

    NVGcolor typeColor = app::views::active().success;
    if (report.tone() == NatReport::Tone::Warning)
        typeColor = app::views::active().warning;

    addResultRow("akira/network/nat_type"_i18n, brls::getStr("akira/network/nat_type_value", *report.type()), typeColor);
    addResultRow("", brls::getStr(report.detailKey()), app::views::active().textDim);
}

void NetworkUtilitiesTab::addResultRow(const std::string& label, const std::string& value, NVGcolor valueColor) {
    auto* row = new brls::Box();
    row->setAxis(brls::Axis::ROW);
    row->setMarginTop(10);

    auto* labelView = new brls::Label();
    labelView->setText(label + ":");
    labelView->setFontSize(20);
    labelView->setTextColor(app::views::active().textDim);
    labelView->setWidth(150);
    row->addView(labelView);

    auto* valueView = new brls::Label();
    valueView->setText(value);
    valueView->setFontSize(20);
    valueView->setTextColor(valueColor);
    row->addView(valueView);

    resultContainer->addView(row);
}

}
