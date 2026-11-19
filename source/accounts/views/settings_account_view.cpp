#include "accounts/views/settings_account_view.hpp"
#include "consoles/views/host_list_tab.hpp"
#include "consoles/views/pair_view.hpp"
#include "accounts/views/profile_setup_view.hpp"
#include "app/views/theme.hpp"
#include "consoles/discovery_manager.hpp"
#include "trophies/trophy_manager.hpp"
#include "play/chiaki_core.hpp"
#include "settings/views/settings_frame_view.hpp"
#include "log/log.hpp"

#include <borealis/core/i18n.hpp>
#include <format>

#include <ctime>
#include <iomanip>
#include <sstream>

namespace akira::accounts::views {

using namespace brls::literals;

static const brls::ButtonStyle BUTTONSTYLE_BLUE = {
    .shadowType              = brls::ShadowType::GENERIC,
    .hideHighlightBackground = true,
    .highlightPadding = "",
    .borderThickness  = "",
    .enabledBackgroundColor = "",
    .enabledLabelColor      = "brls/button/primary_enabled_text",
    .enabledBorderColor     = "",
    .disabledBackgroundColor = "",
    .disabledLabelColor      = "brls/button/primary_disabled_text",
    .disabledBorderColor     = "",
};

static const brls::ButtonStyle BUTTONSTYLE_GREEN = {
    .shadowType              = brls::ShadowType::GENERIC,
    .hideHighlightBackground = true,
    .highlightPadding = "",
    .borderThickness  = "",
    .enabledBackgroundColor = "",
    .enabledLabelColor      = "brls/button/primary_enabled_text",
    .enabledBorderColor     = "",
    .disabledBackgroundColor = "",
    .disabledLabelColor      = "brls/button/primary_disabled_text",
    .disabledBorderColor     = "",
};

SettingsAccountView* SettingsAccountView::currentInstance = nullptr;

SettingsAccountView::SettingsAccountView() {
    this->inflateFromXMLRes("xml/settings/account.xml");

    currentInstance = this;
    settings = settings::SettingsManager::getInstance();

    initAuthSection();

    profileSwitcher = new ProfileSwitcherView();
    profileSwitcher->onProfileChanged = [this]() {
        updateCredentialsDisplay();
        trophiesEnabledCell->setOn(settings->accounts().activeProfileTrophiesEnabled(), false);
        /*
         * A legacy <-> normal switch changes which screens exist, and this frame built
         * its menu bar once in its constructor.
         */
        if (settings::views::SettingsFrameView::currentInstance)
            settings::views::SettingsFrameView::currentInstance->onActiveProfileChanged();
    };
    profileCardSlot->addView(profileSwitcher);

    trophiesEnabledCell->init(
        "akira/settings/trophies_enabled"_i18n,
        settings->accounts().activeProfileTrophiesEnabled(),
        [this](bool isOn) {
            settings->accounts().setActiveProfileTrophiesEnabled(isOn);
            settings->store().save();
        });

    applyLegacyVisibility();

    revealCredentialsBtn->registerClickAction([this](brls::View*) {
        credentialsRevealed = !credentialsRevealed;
        revealCredentialsBtn->setText(credentialsRevealed ? "akira/settings/hide_secrets"_i18n : "akira/settings/reveal_secrets"_i18n);
        updateCredentialsDisplay();
        return true;
    });

    updateCredentialsDisplay();

}

SettingsAccountView::~SettingsAccountView() {
    if (currentInstance == this) {
        currentInstance = nullptr;
    }
}

void SettingsAccountView::willAppear(bool resetState) {
    Box::willAppear(resetState);
    if (profileSwitcher) profileSwitcher->refresh();
    refreshTokenGate.start();
}

void SettingsAccountView::willDisappear(bool resetState) {
    Box::willDisappear(resetState);
    refreshTokenGate.stop();
}

void SettingsAccountView::initAuthSection() {
    std::string currentAccountId = settings->accounts().psnAccountId();
    psnAccountIdInput->init(
        "akira/settings/psn_account_id"_i18n,
        currentAccountId,
        [this](std::string text) {
            std::string current = settings->accounts().psnAccountId();
            if (text == current || text == settings->app().maskAccountName(current))
                return;
            /*
             * chiaki decodes this into a fixed 8-byte buffer, so a wrong-length value
             * only fails much later, at registration, with an error that names nothing.
             */
            if (!text.empty() && !validAccountId(text)) {
                brls::Application::notify("akira/settings/psn_account_id_invalid"_i18n);
                psnAccountIdInput->setValue(current);
                return;
            }
            settings->accounts().setPsnAccountId(text);
            settings->store().save();
            if (profileSwitcher) profileSwitcher->refresh();
            consoles::views::HostListTab::notifyActiveProfileChanged();
            log::info("PSN Account ID set");
        },
        "akira/settings/psn_account_id_placeholder"_i18n,
        "akira/settings/psn_account_id_hint"_i18n
    );

    std::string currentOnlineId = settings->accounts().psnOnlineId();
    psnOnlineIdInput->init(
        "akira/settings/online_id"_i18n,
        currentOnlineId,
        [this](std::string text) {
            std::string current = settings->accounts().psnOnlineId();
            if (text == current || text == settings->app().maskAccountName(current))
                return;
            settings->accounts().setPsnOnlineId(text);
            settings->store().save();
            if (profileSwitcher) profileSwitcher->refresh();
            consoles::views::HostListTab::notifyActiveProfileChanged();
            log::info("PSN Online ID set");
        },
        "",
        ""
    );

    std::string currentPort = std::format("{}", settings->network().companionPort());
    companionPortInput->init(
        "akira/settings/companion_port"_i18n,
        currentPort,
        [this](std::string text) {
            int port = std::atoi(text.c_str());
            if (port > 0 && port <= 65535) {
                settings->network().setCompanionPort(port);
                settings->store().save();
                log::info("Companion port set to {}", port);
            } else {
                brls::Application::notify("akira/settings/invalid_port"_i18n);
            }
        },
        "akira/settings/companion_port_placeholder"_i18n,
        "akira/settings/companion_port_hint"_i18n
    );

    pairBtn->setStyle(&brls::BUTTONSTYLE_PRIMARY);

    pairBtn->registerClickAction([](brls::View*) {
        brls::Application::pushActivity(new brls::Activity(new ProfileSetupView()));
        return true;
    });

    refreshTokenBtn->setStyle(&BUTTONSTYLE_GREEN);

    refreshTokenGate.attach(
        refreshTokenBtn,
        app::views::active().success,
        "akira/settings/refresh_token_btn"_i18n,
        "akira/settings/refresh_token_busy"_i18n,
        "akira/settings/refresh_token_wait",
        []() {
            auto* accounts = play::ChiakiCore::current().accounts();
            return accounts ? accounts->refreshStatus() : ActionStatus{};
        }
    );

    refreshTokenBtn->registerClickAction([this](brls::View* view) {
        if (!refreshTokenGate.isReady()) {
            return true;
        }

        auto* accounts = play::ChiakiCore::current().accounts();
        if (!accounts || accounts->state() == SessionState::NotLinked) {
            brls::Application::notify("akira/settings/no_refresh_token"_i18n);
            return true;
        }

        brls::Application::notify("akira/settings/refreshing_token"_i18n);

        accounts->refreshNow([](bool) {
            brls::Application::notify("akira/settings/token_refreshed"_i18n);
            if (SettingsAccountView::currentInstance) {
                SettingsAccountView::currentInstance->updateCredentialsDisplay();
            }
        });

        refreshTokenGate.apply();

        return true;
    });

    clearPsnBtn->setStyle(&BUTTONSTYLE_BLUE);
    clearPsnBtn->setBackgroundColor(app::views::active().accent);

    clearPsnBtn->registerClickAction([this](brls::View* view) {
        auto* dialog = new brls::Dialog("akira/settings/clear_psn_confirm"_i18n);

        dialog->addButton("akira/common/cancel"_i18n, [dialog]() {
            dialog->close();
        });

        dialog->addButton("akira/settings/clear_all"_i18n, [this, dialog]() {
            dialog->close();
            if (auto* accounts = play::ChiakiCore::current().accounts())
                accounts->forget(settings->accounts().psnAccountId());
            updateCredentialsDisplay();
            brls::Application::notify("akira/settings/psn_data_cleared"_i18n);
        });

        dialog->open();
        return true;
    });
}

void SettingsAccountView::applyLegacyVisibility() {
    if (!settings->accounts().isLegacyProfileActive())
        return;

    /*
     * A legacy profile holds an account id and nothing else, so every credential row
     * would read "Not set" forever and both actions would be no-ops. What stays is the
     * identity, the companion port and Pair - the things that still do something.
     */
    auto hide = [](brls::View* view) {
        if (view)
            view->setVisibility(brls::Visibility::GONE);
    };

    if (trophiesEnabledCell)
        hide(trophiesEnabledCell->getParent());

    hide(revealCredentialsBtn);
    hide(refreshTokenBtn);
    hide(clearPsnBtn);
    hide(credAccessTokenCell);
    hide(credRefreshTokenCell);
    hide(credTokenExpiryCell);
    hide(credNpssoCell);
    hide(credSsoAccessTokenCell);
    hide(credSsoRefreshTokenCell);
    hide(credSsoExpiryCell);
    hide(credDuidCell);
}

bool SettingsAccountView::validAccountId(const std::string& accountId) {
    return libchiaking::AccountId::from_base64(accountId).has_value();
}

std::string SettingsAccountView::censorString(const std::string& str) {
    if (str.empty()) return "akira/common/not_set"_i18n;
    if (str.length() <= 5) return str;
    return "****" + str.substr(str.length() - 5);
}

void SettingsAccountView::updateCredentialsDisplay() {
    bool maskNames = settings->app().hideAccountName() && !credentialsRevealed;
    std::string accountId = settings->accounts().psnAccountId();
    std::string onlineId = settings->accounts().psnOnlineId();
    psnAccountIdInput->setValue(maskNames ? settings->app().maskAccountName(accountId) : accountId);
    psnOnlineIdInput->setValue(maskNames ? settings->app().maskAccountName(onlineId) : onlineId);

    auto* accounts = play::ChiakiCore::current().accounts();
    const Accounts::User user = accounts ? accounts->activeUser() : nullptr;
    const std::vector<libchiaking::users::HeldCredential> held =
        user ? user->held_credentials() : std::vector<libchiaking::users::HeldCredential>{};

    credAccessTokenCell->setText("akira/settings/access_token"_i18n);
    credAccessTokenCell->setDetailText(secretText(user, CredentialKind::remote_play_token));
    credRefreshTokenCell->setText("akira/settings/refresh_token"_i18n);
    credRefreshTokenCell->setDetailText(secretText(user, CredentialKind::remote_play_refresh_token));
    credTokenExpiryCell->setText("akira/settings/token_expires"_i18n);
    credTokenExpiryCell->setDetailText(expiryText(held, CredentialKind::remote_play_token));
    credNpssoCell->setText("akira/settings/npsso"_i18n);
    credNpssoCell->setDetailText(secretText(user, CredentialKind::sign_in_seed));
    credSsoAccessTokenCell->setText("akira/settings/sso_access_token"_i18n);
    credSsoAccessTokenCell->setDetailText(secretText(user, CredentialKind::app_token));
    credSsoRefreshTokenCell->setText("akira/settings/sso_refresh_token"_i18n);
    credSsoRefreshTokenCell->setDetailText(secretText(user, CredentialKind::app_refresh_token));
    credSsoExpiryCell->setText("akira/settings/sso_expires"_i18n);
    credSsoExpiryCell->setDetailText(expiryText(held, CredentialKind::app_token));
    credDuidCell->setText("akira/settings/duid"_i18n);
    credDuidCell->setDetailText(censorString(settings->cloud().duid()));
}

std::string SettingsAccountView::secretText(const Accounts::User& user, CredentialKind kind) {
    if (!user)
        return "akira/common/not_set"_i18n;
    const auto secret = user->reveal(kind);
    if (!secret)
        return "akira/common/not_set"_i18n;
    const std::string_view text = secret->view();
    if (!credentialsRevealed)
        return text.length() <= 5 ? std::string(text) : "****" + std::string(text.substr(text.length() - 5));
    return text.length() > 40 ? std::string(text.substr(0, 36)) + "..." : std::string(text);
}

std::string SettingsAccountView::expiryText(const std::vector<libchiaking::users::HeldCredential>& held,
                                            CredentialKind kind) {
    const auto found = std::ranges::find(held, kind, &libchiaking::users::HeldCredential::kind);
    if (found == held.end() || !found->expires_at)
        return "akira/common/not_set"_i18n;
    const std::time_t expTime = std::chrono::system_clock::to_time_t(*found->expires_at);
    std::tm* tm = std::localtime(&expTime);
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
    return found->state == libchiaking::users::HeldCredential::State::expired
        ? oss.str() + "akira/settings/expired_suffix"_i18n
        : oss.str();
}

}
