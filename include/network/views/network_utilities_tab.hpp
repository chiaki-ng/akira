#ifndef AKIRA_NETWORK_UTILITIES_TAB_HPP
#define AKIRA_NETWORK_UTILITIES_TAB_HPP

#include <atomic>
#include <memory>

#include <borealis.hpp>
#include "network/stun_client.hpp"
#include "app/views/theme.hpp"

namespace akira::network::views {

class NetworkUtilitiesTab : public brls::Box {
public:
    NetworkUtilitiesTab();
    ~NetworkUtilitiesTab() override;

    static brls::View* create();

private:
    BRLS_BIND(brls::Box, resultContainer, "network/resultContainer");
    BRLS_BIND(brls::Button, checkNatBtn, "network/checkNatBtn");
    BRLS_BIND(brls::Box, wgStatusContainer, "network/wgStatusContainer");
    BRLS_BIND(brls::Box, wgButtonContainer, "network/wgButtonContainer");

    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);

    bool isChecking = false;
    bool isWgConnecting = false;
    brls::Button* wgConnectBtn = nullptr;
    brls::Button* wgDisconnectBtn = nullptr;

    void onCheckNatClicked();
    void displayResult(const StunResult& result);
    void clearResults();
    void addResultRow(const std::string& label, const std::string& value,
                      NVGcolor valueColor = app::views::active().textMuted);
    void addCompatibilityTable();

    void initWireGuardUI();
    void updateWireGuardStatus();
    void onWgConnectClicked();
    void onWgDisconnectClicked();
};

}

#endif
