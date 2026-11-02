#include "views/network_utilities_tab.hpp"
#include "ui/theme.hpp"
#include "core/discovery_manager.hpp"
#include "core/wireguard_manager.hpp"
#include <format>
#include <vector>
#include <borealis/core/i18n.hpp>
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
    auto& wg = WireGuardManager::instance();

    if (!wg.configExists()) {
        auto* noConfigLabel = new brls::Label();
        noConfigLabel->setText("akira/network/wg_no_config"_i18n);
        noConfigLabel->setFontSize(18);
        noConfigLabel->setTextColor(akira::ui::active().warning);
        wgStatusContainer->addView(noConfigLabel);

        auto* pathLabel = new brls::Label();
        pathLabel->setText("akira/network/wg_config_path"_i18n);
        pathLabel->setFontSize(14);
        pathLabel->setTextColor(akira::ui::active().textDim);
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
    auto& wg = WireGuardManager::instance();

    wgStatusContainer->clearViews();

    auto* statusRow = new brls::Box();
    statusRow->setAxis(brls::Axis::ROW);
    statusRow->setMarginBottom(10);

    auto* statusLabel = new brls::Label();
    statusLabel->setText("akira/network/wg_status"_i18n);
    statusLabel->setFontSize(18);
    statusLabel->setTextColor(akira::ui::active().textDim);
    statusRow->addView(statusLabel);

    auto* statusValue = new brls::Label();
    if (isWgConnecting) {
        statusValue->setText("akira/network/wg_connecting"_i18n);
        statusValue->setTextColor(akira::ui::active().warning);
    } else if (wg.isConnected()) {
        statusValue->setText("akira/network/wg_connected"_i18n);
        statusValue->setTextColor(akira::ui::active().success);
    } else {
        statusValue->setText("akira/network/wg_disconnected"_i18n);
        statusValue->setTextColor(akira::ui::active().danger);
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
        ipLabel->setTextColor(akira::ui::active().textDim);
        ipRow->addView(ipLabel);

        auto* ipValue = new brls::Label();
        ipValue->setText(wg.getTunnelIP());
        ipValue->setFontSize(16);
        ipValue->setTextColor(akira::ui::active().textMuted);
        ipRow->addView(ipValue);

        wgStatusContainer->addView(ipRow);
    }

    if (!wg.getLastError().empty() && !wg.isConnected()) {
        auto* errorLabel = new brls::Label();
        errorLabel->setText(brls::getStr("akira/network/wg_error", wg.getLastError()));
        errorLabel->setFontSize(14);
        errorLabel->setTextColor(akira::ui::active().danger);
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
        bool success = WireGuardManager::instance().connect();
        brls::sync([this, guard, success]() {
            if (!*guard)
                return;
            isWgConnecting = false;
            if (success)
                DiscoveryManager::getInstance()->restartSearch();
            updateWireGuardStatus();
        });
    });
}

void NetworkUtilitiesTab::onWgDisconnectClicked() {
    WireGuardManager::instance().disconnect();
    DiscoveryManager::getInstance()->restartSearch();
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
    addResultRow("", "akira/network/testing"_i18n, akira::ui::active().textDim);

    StunResult result = StunClient::detectNATType();

    isChecking = false;
    displayResult(result);
}

void NetworkUtilitiesTab::clearResults() {
    resultContainer->clearViews();
}

void NetworkUtilitiesTab::displayResult(const StunResult& result) {
    clearResults();

    if (!result.error.empty() && result.type == NATType::Unknown) {
        addResultRow("akira/common/error"_i18n, result.error, akira::ui::active().danger);
        return;
    }

    std::string typeStr = StunClient::natTypeToString(result.type);
    std::string descStr = StunClient::natTypeDescription(result.type);
    NVGcolor typeColor;
    if (result.type == NATType::FullCone || result.type == NATType::OpenInternet ||
        result.type == NATType::RestrictedCone || result.type == NATType::PortRestrictedCone) {
        typeColor = akira::ui::active().success;
    } else if (result.type == NATType::UDPBlocked) {
        typeColor = akira::ui::active().danger;
    } else if (result.type == NATType::Symmetric || result.type == NATType::SymmetricPortOnly) {
        typeColor = akira::ui::active().warning;
    } else {
        typeColor = akira::ui::active().textDim;
    }

    addResultRow("akira/network/nat_type"_i18n, typeStr, typeColor);
    addResultRow("", descStr, akira::ui::active().textDim);

    std::string filteringStr = StunClient::filteringTypeToString(result.filtering);
    NVGcolor filteringColor;
    if (result.filtering == FilteringType::EndpointIndependent) {
        filteringColor = akira::ui::active().success;
    } else if (result.filtering == FilteringType::AddressDependent ||
               result.filtering == FilteringType::AddressPortDependent) {
        filteringColor = akira::ui::active().warning;
    } else {
        filteringColor = akira::ui::active().textDim;
    }

    addResultRow("akira/network/filtering"_i18n, filteringStr, filteringColor);

    if (!result.externalIP.empty()) {
        addResultRow("akira/network/external_ip"_i18n, result.externalIP);
    }

    if (result.externalPort > 0) {
        addResultRow("akira/network/external_port"_i18n, std::format("{}", result.externalPort));
    }

    addCompatibilityTable();
}

void NetworkUtilitiesTab::addCompatibilityTable() {
    auto* spacer = new brls::Box();
    spacer->setHeight(30);
    resultContainer->addView(spacer);

    auto* header = new brls::Label();
    header->setText("akira/network/holepunch_compat"_i18n);
    header->setFontSize(22);
    header->setTextColor(akira::ui::active().accent);
    header->setMarginBottom(10);
    resultContainer->addView(header);

    auto* note = new brls::Label();
    note->setText("akira/network/holepunch_note"_i18n);
    note->setFontSize(16);
    note->setTextColor(akira::ui::active().textDim);
    note->setMarginBottom(15);
    resultContainer->addView(note);

    auto* mappingHeader = new brls::Label();
    mappingHeader->setText("akira/network/mapping"_i18n);
    mappingHeader->setFontSize(18);
    mappingHeader->setTextColor(akira::ui::active().textDim);
    mappingHeader->setMarginBottom(8);
    resultContainer->addView(mappingHeader);

    struct CompatRow {
        std::string switchNat;
        std::string ps5Nat;
        std::string result;
        NVGcolor color;
    };

    std::vector<CompatRow> mappingRows = {
        {"akira/network/cone"_i18n, "akira/network/cone"_i18n, "akira/network/works"_i18n, akira::ui::active().success},
        {"akira/network/cone"_i18n, "akira/network/symmetric"_i18n, "akira/network/may_work"_i18n, akira::ui::active().warning},
        {"akira/network/symmetric"_i18n, "akira/network/cone"_i18n, "akira/network/may_work"_i18n, akira::ui::active().warning},
        {"akira/network/symmetric"_i18n, "akira/network/symmetric"_i18n, "akira/network/unlikely"_i18n, akira::ui::active().danger},
    };

    auto* headerRow = new brls::Box();
    headerRow->setAxis(brls::Axis::ROW);
    headerRow->setMarginBottom(5);
    headerRow->setWidthPercentage(100);

    auto* col1 = new brls::Label();
    col1->setText("akira/network/col_switch"_i18n);
    col1->setFontSize(14);
    col1->setTextColor(akira::ui::active().textMuted);
    col1->setWidthPercentage(40);
    headerRow->addView(col1);

    auto* col2 = new brls::Label();
    col2->setText("akira/network/col_ps5"_i18n);
    col2->setFontSize(14);
    col2->setTextColor(akira::ui::active().textMuted);
    col2->setWidthPercentage(40);
    headerRow->addView(col2);

    auto* col3 = new brls::Label();
    col3->setText("akira/network/col_result"_i18n);
    col3->setFontSize(14);
    col3->setTextColor(akira::ui::active().textMuted);
    col3->setWidthPercentage(20);
    headerRow->addView(col3);

    resultContainer->addView(headerRow);

    for (const auto& row : mappingRows) {
        auto* tableRow = new brls::Box();
        tableRow->setAxis(brls::Axis::ROW);
        tableRow->setMarginTop(6);
        tableRow->setWidthPercentage(100);

        auto* c1 = new brls::Label();
        c1->setText(row.switchNat);
        c1->setFontSize(14);
        c1->setTextColor(akira::ui::active().textMuted);
        c1->setWidthPercentage(40);
        tableRow->addView(c1);

        auto* c2 = new brls::Label();
        c2->setText(row.ps5Nat);
        c2->setFontSize(14);
        c2->setTextColor(akira::ui::active().textMuted);
        c2->setWidthPercentage(40);
        tableRow->addView(c2);

        auto* c3 = new brls::Label();
        c3->setText(row.result);
        c3->setFontSize(14);
        c3->setTextColor(row.color);
        c3->setWidthPercentage(20);
        tableRow->addView(c3);

        resultContainer->addView(tableRow);
    }

    auto* spacer2 = new brls::Box();
    spacer2->setHeight(20);
    resultContainer->addView(spacer2);

    auto* filteringHeader = new brls::Label();
    filteringHeader->setText("akira/network/filtering_cone"_i18n);
    filteringHeader->setFontSize(18);
    filteringHeader->setTextColor(akira::ui::active().textDim);
    filteringHeader->setMarginBottom(8);
    resultContainer->addView(filteringHeader);

    std::vector<CompatRow> filteringRows = {
        {"akira/network/open"_i18n, "akira/network/open"_i18n, "akira/network/works"_i18n, akira::ui::active().success},
        {"akira/network/open"_i18n, "akira/network/restricted"_i18n, "akira/network/works"_i18n, akira::ui::active().success},
        {"akira/network/restricted"_i18n, "akira/network/open"_i18n, "akira/network/works"_i18n, akira::ui::active().success},
        {"akira/network/restricted"_i18n, "akira/network/restricted"_i18n, "akira/network/may_fail"_i18n, akira::ui::active().warning},
    };

    auto* headerRow2 = new brls::Box();
    headerRow2->setAxis(brls::Axis::ROW);
    headerRow2->setMarginBottom(5);
    headerRow2->setWidthPercentage(100);

    auto* col1b = new brls::Label();
    col1b->setText("akira/network/col_switch"_i18n);
    col1b->setFontSize(14);
    col1b->setTextColor(akira::ui::active().textMuted);
    col1b->setWidthPercentage(40);
    headerRow2->addView(col1b);

    auto* col2b = new brls::Label();
    col2b->setText("akira/network/col_ps5"_i18n);
    col2b->setFontSize(14);
    col2b->setTextColor(akira::ui::active().textMuted);
    col2b->setWidthPercentage(40);
    headerRow2->addView(col2b);

    auto* col3b = new brls::Label();
    col3b->setText("akira/network/col_result"_i18n);
    col3b->setFontSize(14);
    col3b->setTextColor(akira::ui::active().textMuted);
    col3b->setWidthPercentage(20);
    headerRow2->addView(col3b);

    resultContainer->addView(headerRow2);

    for (const auto& row : filteringRows) {
        auto* tableRow = new brls::Box();
        tableRow->setAxis(brls::Axis::ROW);
        tableRow->setMarginTop(6);
        tableRow->setWidthPercentage(100);

        auto* c1 = new brls::Label();
        c1->setText(row.switchNat);
        c1->setFontSize(14);
        c1->setTextColor(akira::ui::active().textMuted);
        c1->setWidthPercentage(40);
        tableRow->addView(c1);

        auto* c2 = new brls::Label();
        c2->setText(row.ps5Nat);
        c2->setFontSize(14);
        c2->setTextColor(akira::ui::active().textMuted);
        c2->setWidthPercentage(40);
        tableRow->addView(c2);

        auto* c3 = new brls::Label();
        c3->setText(row.result);
        c3->setFontSize(14);
        c3->setTextColor(row.color);
        c3->setWidthPercentage(20);
        tableRow->addView(c3);

        resultContainer->addView(tableRow);
    }
}

void NetworkUtilitiesTab::addResultRow(const std::string& label, const std::string& value, NVGcolor valueColor) {
    auto* row = new brls::Box();
    row->setAxis(brls::Axis::ROW);
    row->setMarginTop(10);

    auto* labelView = new brls::Label();
    labelView->setText(label + ":");
    labelView->setFontSize(20);
    labelView->setTextColor(akira::ui::active().textDim);
    labelView->setWidth(150);
    row->addView(labelView);

    auto* valueView = new brls::Label();
    valueView->setText(value);
    valueView->setFontSize(20);
    valueView->setTextColor(valueColor);
    row->addView(valueView);

    resultContainer->addView(row);
}
