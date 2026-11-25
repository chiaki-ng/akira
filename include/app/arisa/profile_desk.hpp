#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <libchiaking/users.hpp>

#include "accounts/account_desk.hpp"
#include "accounts/accounts.hpp"
#include "accounts/profile.hpp"
#include "app/services.hpp"
#include "consoles/pair_advertiser.hpp"
#include "consoles/pair_listener.hpp"

namespace akira::app {
class Notifier;
}

namespace akira::app::arisa {

class ProfileDesk final : public accounts::AccountDesk {
public:
    ProfileDesk(Services& services, Notifier& notifier, std::function<void()> profileChanged);
    ~ProfileDesk() override;

    ProfileDesk(const ProfileDesk&) = delete;
    ProfileDesk& operator=(const ProfileDesk&) = delete;

    auto entries() const -> std::vector<Entry> override;
    auto identity(bool revealed) const -> Identity override;
    auto credentials(bool revealed) const -> std::vector<Credential> override;
    void summary(std::function<void(Summary)> shown) override;

    void switchTo(std::int64_t id) override;
    auto signInExpired() const -> bool override;
    void remove(std::int64_t id) override;
    [[nodiscard]] auto setAccountId(const std::string& text) -> bool override;
    void setOnlineId(const std::string& text) override;
    void setTrophies(bool enabled) override;
    [[nodiscard]] auto setCompanionPort(int port) -> bool override;

    auto refreshState() const -> Refresh override;
    void refresh(std::function<void(bool)> done) override;
    void clearPsn() override;

    [[nodiscard]] auto createLegacy(const std::string& accountId, const std::string& name) -> bool override;
    void signInWithQr(bool createProfile, Qr progress) override;
    void cancelSignIn() override;
    auto openPairing(bool createProfile, Paired step) -> Pairing override;
    auto renewPairing() -> Pairing override;
    void closePairing() override;

private:
    using CredentialKind = libchiaking::users::CredentialKind;

    static constexpr std::size_t shownTail = 5;
    static constexpr std::size_t revealedLimit = 40;
    static constexpr int highestPort = 65535;

    static auto kindOf(const accounts::Profile& profile) -> Kind;
    static auto failureKey(libchiaking::users::SignInErrorKind kind) -> std::string;
    static auto pairStep(consoles::ListenerEvent event) -> PairStep;
    static auto validAccountId(const std::string& text) -> bool;
    static auto tail(std::string_view text) -> std::string;
    auto masked(const std::string& name, bool revealed) const -> std::string;
    auto secret(const accounts::Accounts::User& user, CredentialKind kind, bool revealed) const -> std::optional<std::string>;
    static auto expiry(const std::vector<libchiaking::users::HeldCredential>& held, CredentialKind kind) -> Credential;
    auto cloudKey() const -> std::string;
    void listen();
    void applyCredentials(const consoles::PairedCredentials& credentials, bool createProfile);
    void adopt(std::int64_t profileId, const accounts::Accounts::User& user);
    void finishQr(accounts::SignInOutcome outcome, const accounts::Accounts::User& user, bool createProfile, const Qr& progress);

    Services& services;
    Notifier& notifier;
    std::function<void()> profileChanged;
    std::optional<consoles::PairListener> listener;
    std::optional<consoles::PairAdvertiser> advertiser;
    Paired paired;
    bool pairCreates = false;
    int pairPort = 0;
    std::string pairAddress;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

}
