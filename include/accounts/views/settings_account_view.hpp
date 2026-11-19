#ifndef AKIRA_SETTINGS_ACCOUNT_VIEW_HPP
#define AKIRA_SETTINGS_ACCOUNT_VIEW_HPP

#include <borealis.hpp>
#include <borealis/views/cells/cell_detail.hpp>
#include <borealis/views/cells/cell_input.hpp>
#include <borealis/views/cells/cell_selector.hpp>
#include <borealis/views/cells/cell_bool.hpp>
#include <string>

#include "app/view_services.hpp"
#include "accounts/views/psn_action_button.hpp"
#include "accounts/views/profile_switcher_view.hpp"

namespace akira::accounts::views {

class SettingsAccountView : public brls::Box {
public:
    SettingsAccountView();
    ~SettingsAccountView() override;

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;

    static SettingsAccountView* currentInstance;

private:
    BRLS_BIND(brls::Box, profileCardSlot, "settings/profileCardSlot");
    BRLS_BIND(brls::BooleanCell, trophiesEnabledCell, "settings/trophiesEnabled");
    BRLS_BIND(brls::InputCell, psnAccountIdInput, "settings/psnAccountId");
    BRLS_BIND(brls::InputCell, psnOnlineIdInput, "settings/psnOnlineId");
    BRLS_BIND(brls::InputCell, companionPortInput, "settings/companionPort");
    BRLS_BIND(brls::Button, pairBtn, "settings/pairBtn");
    BRLS_BIND(brls::Button, refreshTokenBtn, "settings/refreshTokenBtn");
    BRLS_BIND(brls::Button, clearPsnBtn, "settings/clearPsnBtn");
    BRLS_BIND(brls::Button, revealCredentialsBtn, "settings/revealCredentials");
    BRLS_BIND(brls::DetailCell, credAccessTokenCell, "settings/credAccessToken");
    BRLS_BIND(brls::DetailCell, credRefreshTokenCell, "settings/credRefreshToken");
    BRLS_BIND(brls::DetailCell, credTokenExpiryCell, "settings/credTokenExpiry");
    BRLS_BIND(brls::DetailCell, credNpssoCell, "settings/credNpsso");
    BRLS_BIND(brls::DetailCell, credSsoAccessTokenCell, "settings/credSsoAccessToken");
    BRLS_BIND(brls::DetailCell, credSsoRefreshTokenCell, "settings/credSsoRefreshToken");
    BRLS_BIND(brls::DetailCell, credSsoExpiryCell, "settings/credSsoExpiry");
    BRLS_BIND(brls::DetailCell, credDuidCell, "settings/credDuid");

    PsnActionButton refreshTokenGate;

    app::Services* services = nullptr;
    ProfileSwitcherView* profileSwitcher = nullptr;
    bool credentialsRevealed = false;

    void initAuthSection();
    void applyLegacyVisibility();
    static bool validAccountId(const std::string& accountId);
    void updateCredentialsDisplay();
    std::string censorString(const std::string& str);

    using CredentialKind = libchiaking::users::CredentialKind;
    std::string secretText(const Accounts::User& user, CredentialKind kind);
    static std::string expiryText(const std::vector<libchiaking::users::HeldCredential>& held, CredentialKind kind);
};

}

#endif // AKIRA_SETTINGS_ACCOUNT_VIEW_HPP
