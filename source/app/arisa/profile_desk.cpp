#include "app/arisa/profile_desk.hpp"

#include <algorithm>
#include <ctime>
#include <utility>

#include "accounts/account_settings.hpp"
#include "accounts/profile_switch.hpp"
#include "app/app_settings.hpp"
#include "app/notifier.hpp"
#include "app/strings.hpp"
#include "app/ui_dispatcher.hpp"
#include "cloud/cloud_settings.hpp"
#include "consoles/host_catalog.hpp"
#include "log/log.hpp"
#include "network/network_settings.hpp"
#include "play/chiaki_core.hpp"
#include "settings/settings_store.hpp"
#include "trophies/trophy_manager.hpp"
#if AKIRA_CLOUD
#include "cloud/service.hpp"
#endif

namespace akira::app::arisa {

ProfileDesk::ProfileDesk(Services& services, Notifier& notifier, std::function<void()> profileChanged)
    : services(services)
    , notifier(notifier)
    , profileChanged(std::move(profileChanged))
{
}

ProfileDesk::~ProfileDesk()
{
    *alive = false;
    closePairing();
    cancelSignIn();
}

auto ProfileDesk::kindOf(const accounts::Profile& profile) -> Kind
{
    if (profile.legacy)
        return Kind::Legacy;
    return profile.isRemote() ? Kind::Remote : Kind::Local;
}

auto ProfileDesk::masked(const std::string& name, bool revealed) const -> std::string
{
    if (revealed || !services.appSettings.hideAccountName())
        return name;
    return services.appSettings.maskAccountName(name);
}

auto ProfileDesk::entries() const -> std::vector<Entry>
{
    std::vector<Entry> listed;
    const std::int64_t active = services.accounts.activeProfileId();
    for (const accounts::Profile& profile : services.accounts.profilesSnapshot())
        listed.push_back(Entry{ .id = profile.id,
            .name = profile.label().empty() ? std::string() : services.appSettings.maskAccountName(profile.label()),
            .kind = kindOf(profile),
            .active = profile.id == active,
            .avatarUrl = profile.avatarUrl });
    return listed;
}

auto ProfileDesk::identity(bool revealed) const -> Identity
{
    return Identity{
        .accountId = masked(services.accounts.psnAccountId(), revealed),
        .onlineId = masked(services.accounts.psnOnlineId(), revealed),
        .legacy = services.accounts.isLegacyProfileActive(),
        .trophies = services.accounts.activeProfileTrophiesEnabled(),
        .companionPort = services.networkSettings.companionPort(),
    };
}

auto ProfileDesk::tail(std::string_view text) -> std::string
{
    if (text.length() <= shownTail)
        return std::string(text);
    return "****" + std::string(text.substr(text.length() - shownTail));
}

auto ProfileDesk::secret(const accounts::Accounts::User& user, CredentialKind kind, bool revealed) const
    -> std::optional<std::string>
{
    if (!user)
        return std::nullopt;
    const auto held = user->reveal(kind);
    if (!held)
        return std::nullopt;
    const std::string_view text = held->view();
    if (!revealed)
        return tail(text);
    if (text.length() > revealedLimit)
        return std::string(text.substr(0, revealedLimit - 4)) + "...";
    return std::string(text);
}

auto ProfileDesk::expiry(const std::vector<libchiaking::users::HeldCredential>& held, CredentialKind kind) -> Credential
{
    const auto found = std::ranges::find(held, kind, &libchiaking::users::HeldCredential::kind);
    if (found == held.end() || !found->expires_at)
        return {};
    const std::time_t when = std::chrono::system_clock::to_time_t(*found->expires_at);
    std::tm local{};
    localtime_r(&when, &local);
    char text[32]{};
    if (std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &local) == 0)
        return {};
    return Credential{ .shown = std::string(text), .expired = found->state == libchiaking::users::HeldCredential::State::expired };
}

auto ProfileDesk::credentials(bool revealed) const -> std::vector<Credential>
{
    accounts::Accounts* psn = services.core.accounts();
    const accounts::Accounts::User user = psn ? psn->activeUser() : nullptr;
    const std::vector<libchiaking::users::HeldCredential> held =
        user ? user->held_credentials() : std::vector<libchiaking::users::HeldCredential>{};
    Credential tokenExpiry = expiry(held, CredentialKind::remote_play_token);
    tokenExpiry.secret = Secret::TokenExpires;
    Credential ssoExpiry = expiry(held, CredentialKind::app_token);
    ssoExpiry.secret = Secret::SsoExpires;
    const std::string& duid = services.cloudSettings.duid();
    return {
        Credential{ .secret = Secret::AccessToken, .shown = secret(user, CredentialKind::remote_play_token, revealed) },
        Credential{ .secret = Secret::RefreshToken, .shown = secret(user, CredentialKind::remote_play_refresh_token, revealed) },
        tokenExpiry,
        Credential{ .secret = Secret::Npsso, .shown = secret(user, CredentialKind::sign_in_seed, revealed) },
        Credential{ .secret = Secret::SsoAccessToken, .shown = secret(user, CredentialKind::app_token, revealed) },
        Credential{ .secret = Secret::SsoRefreshToken, .shown = secret(user, CredentialKind::app_refresh_token, revealed) },
        ssoExpiry,
        Credential{ .secret = Secret::Duid, .shown = duid.empty() ? std::nullopt : std::optional<std::string>(tail(duid)) },
    };
}

auto ProfileDesk::cloudKey() const -> std::string
{
#if AKIRA_CLOUD
    switch (services.cloud.snapshotForActiveProfile().status.availability)
    {
        case cloud::Availability::Ready: return "akira/cloud/chip_ready";
        case cloud::Availability::CatalogAvailable: return "akira/cloud/chip_catalog";
        case cloud::Availability::SubscriptionRequired: return "akira/cloud/chip_unavailable";
        case cloud::Availability::Warning: return "akira/cloud/chip_warning";
        case cloud::Availability::Empty: return "akira/cloud/chip_empty";
        case cloud::Availability::NeedsPairing: return "akira/cloud/chip_pair";
        case cloud::Availability::Checking: return "akira/cloud/chip_checking";
        case cloud::Availability::LaunchBlocked:
        case cloud::Availability::Error: return "akira/cloud/chip_error";
        case cloud::Availability::NoProfile: break;
    }
#endif
    return {};
}

void ProfileDesk::summary(std::function<void(Summary)> shown)
{
    const accounts::Profile* active = services.accounts.activeProfile();
    if (!active)
    {
        shown(Summary{});
        return;
    }
    auto made = std::make_shared<Summary>(Summary{
        .name = active->label().empty() ? std::string() : services.appSettings.maskAccountName(active->label()),
        .kind = kindOf(*active),
        .cloudKey = active->legacy ? std::string() : cloudKey(),
        .avatarUrl = active->avatarUrl,
    });
    shown(*made);
    if (active->legacy)
        return;
    services.trophies.fetchProfile(false,
        [this, guard = alive, made, shown](const trophies::TrophyManager::Profile& profile) {
            if (!*guard)
                return;
            if (!profile.online_id.empty())
                made->name = services.appSettings.maskAccountName(profile.online_id);
            made->plus = profile.membership.is_ps_plus_member().value_or(false);
            if (profile.avatar_url)
                made->avatarUrl = *profile.avatar_url;
            shown(*made);
        },
        [](accounts::Status, const std::string&) {});
    services.trophies.fetchSummary(false,
        [guard = alive, made, shown](const trophies::TrophyManager::Summary& earned) {
            if (!*guard)
                return;
            made->trophies = Trophies{ .level = static_cast<int>(earned.level),
                .progress = std::min(100, static_cast<int>(earned.progress)),
                .platinum = static_cast<int>(earned.earned.platinum),
                .gold = static_cast<int>(earned.earned.gold),
                .silver = static_cast<int>(earned.earned.silver),
                .bronze = static_cast<int>(earned.earned.bronze) };
            shown(*made);
        },
        [](accounts::Status, const std::string&) {});
}

void ProfileDesk::switchTo(std::int64_t id)
{
    accounts::ProfileSwitch(services.accounts, services.store, [this] { services.trophies.onActiveProfileChanged(); }).to(id);
#if AKIRA_CLOUD
    services.cloud.refreshActiveProfile(false);
#endif
    if (profileChanged)
        profileChanged();
}

auto ProfileDesk::signInExpired() const -> bool
{
    accounts::Accounts* psn = services.core.accounts();
    return psn && psn->state() == accounts::SessionState::Expired;
}

void ProfileDesk::remove(std::int64_t id)
{
    std::string account;
    services.accounts.readProfile(id, [&account](const accounts::Profile& profile) { account = profile.accountId; });
    services.accounts.removeProfile(id);
    services.hosts.forgetProfile(id);
    const bool shared = std::ranges::any_of(services.accounts.profiles(),
        [&account](const accounts::Profile& profile) { return profile.accountId == account; });
    if (!account.empty() && !shared)
        if (accounts::Accounts* psn = services.core.accounts())
            psn->forget(account);
    services.store.save();
    services.trophies.onActiveProfileChanged();
    if (profileChanged)
        profileChanged();
}

auto ProfileDesk::validAccountId(const std::string& text) -> bool
{
    return libchiaking::AccountId::from_base64(text).has_value();
}

auto ProfileDesk::setAccountId(const std::string& text) -> bool
{
    if (!text.empty() && !validAccountId(text))
        return false;
    services.accounts.setPsnAccountId(text);
    services.store.save();
    if (profileChanged)
        profileChanged();
    log::info("[ACCOUNTS] PSN account ID set");
    return true;
}

void ProfileDesk::setOnlineId(const std::string& text)
{
    services.accounts.setPsnOnlineId(text);
    services.store.save();
    if (profileChanged)
        profileChanged();
    log::info("[ACCOUNTS] PSN online ID set");
}

void ProfileDesk::setTrophies(bool enabled)
{
    services.accounts.setActiveProfileTrophiesEnabled(enabled);
    services.store.save();
}

auto ProfileDesk::setCompanionPort(int port) -> bool
{
    if (port <= 0 || port > highestPort)
        return false;
    services.networkSettings.setCompanionPort(port);
    services.store.save();
    log::info("[ACCOUNTS] companion port set to {}", port);
    return true;
}

auto ProfileDesk::refreshState() const -> Refresh
{
    accounts::Accounts* psn = services.core.accounts();
    if (!psn || psn->state() == accounts::SessionState::NotLinked)
        return Refresh{ .state = RefreshState::NotLinked };
    const accounts::ActionStatus status = psn->refreshStatus();
    switch (status.state)
    {
        case accounts::ActionState::Busy: return Refresh{ .state = RefreshState::Busy };
        case accounts::ActionState::CoolingDown:
            return Refresh{ .state = RefreshState::CoolingDown, .seconds = status.secondsRemaining };
        case accounts::ActionState::Ready: break;
    }
    return Refresh{ .state = RefreshState::Ready };
}

void ProfileDesk::refresh(std::function<void(bool)> done)
{
    if (accounts::Accounts* psn = services.core.accounts())
        psn->refreshNow(std::move(done));
}

void ProfileDesk::clearPsn()
{
    if (accounts::Accounts* psn = services.core.accounts())
        psn->forget(services.accounts.psnAccountId());
}

auto ProfileDesk::createLegacy(const std::string& accountId, const std::string& name) -> bool
{
    if (!validAccountId(accountId) || name.empty())
        return false;
    accounts::Profile profile;
    profile.legacy = true;
    profile.accountId = accountId;
    profile.onlineId = name;
    services.accounts.setActiveProfileId(services.accounts.addProfile(profile));
    services.accounts.refreshLegacyGate();
    services.store.save();
    if (profileChanged)
        profileChanged();
    return true;
}

auto ProfileDesk::failureKey(libchiaking::users::SignInErrorKind kind) -> std::string
{
    using Kind = libchiaking::users::SignInErrorKind;
    switch (kind)
    {
        case Kind::declined: return "akira/qr_sign_in/status_declined";
        case Kind::expired: return "akira/qr_sign_in/status_expired";
        case Kind::transport: return "akira/qr_sign_in/status_offline";
        case Kind::rate_limited: return "akira/qr_sign_in/status_rate_limited";
        case Kind::verification_required: return "akira/qr_sign_in/status_verification";
        case Kind::layout_changed: return "akira/qr_sign_in/status_layout_changed";
        case Kind::cancelled: return "akira/qr_sign_in/status_cancelled";
        case Kind::invalid_request:
        case Kind::rejected:
        case Kind::unavailable:
        case Kind::malformed: break;
    }
    return "akira/qr_sign_in/status_failed";
}

void ProfileDesk::signInWithQr(bool createProfile, Qr progress)
{
    accounts::Accounts* psn = services.core.accounts();
    if (!psn)
    {
        progress(QrProgress{ .step = QrStep::Failed, .failureKey = "akira/qr_sign_in/status_failed" });
        return;
    }
    progress(QrProgress{ .step = QrStep::Preparing });
    psn->signInWithQr(
        [guard = alive, progress](const libchiaking::users::QrSignInState& state) {
            if (!*guard)
                return;
            using Step = libchiaking::users::QrSignInStep;
            switch (state.step())
            {
                case Step::preparing: progress(QrProgress{ .step = QrStep::Preparing }); return;
                case Step::awaiting_approval:
                    progress(QrProgress{ .step = QrStep::Waiting, .url = std::string(state.approval_url()), .expires = state.expires_at() });
                    return;
                case Step::signing_in: progress(QrProgress{ .step = QrStep::SigningIn }); return;
                case Step::signed_in: return;
                case Step::failed:
                    progress(QrProgress{ .step = QrStep::Failed,
                        .failureKey = failureKey(state.failure().value_or(libchiaking::users::SignInErrorKind::unavailable)) });
                    return;
            }
        },
        [this, guard = alive, createProfile, progress](accounts::SignInOutcome outcome, const accounts::Accounts::User& user) {
            if (*guard)
                finishQr(outcome, user, createProfile, progress);
        });
}

void ProfileDesk::finishQr(accounts::SignInOutcome outcome, const accounts::Accounts::User& user, bool createProfile,
    const Qr& progress)
{
    if (outcome != accounts::SignInOutcome::SignedIn || !user)
    {
        log::warning("[ACCOUNTS] QR sign-in ended without a user, outcome {}", static_cast<int>(outcome));
        if (outcome != accounts::SignInOutcome::Superseded)
            progress(QrProgress{ .step = QrStep::Failed, .failureKey = "akira/qr_sign_in/status_failed" });
        return;
    }
    if (createProfile || !services.accounts.activeProfile())
        services.accounts.setActiveProfileId(services.accounts.addProfile(accounts::Profile{}));
    const accounts::Profile* profile = services.accounts.activeProfile();
    if (!profile)
        return;
    if (profile->legacy)
    {
        log::warning("[ACCOUNTS] rejecting the QR sign-in: profile {} is legacy", profile->id);
        progress(QrProgress{ .step = QrStep::Failed, .failureKey = "akira/pair/mode_mismatch" });
        return;
    }
    adopt(profile->id, user);
    progress(QrProgress{ .step = QrStep::SignedIn, .onlineId = user->online_id() });
}

void ProfileDesk::cancelSignIn()
{
    if (accounts::Accounts* psn = services.core.accounts())
        psn->cancelSignIn();
}

auto ProfileDesk::pairStep(consoles::ListenerEvent event) -> PairStep
{
    switch (event)
    {
        case consoles::ListenerEvent::Listening: return PairStep::Waiting;
        case consoles::ListenerEvent::ClientConnected: return PairStep::Connecting;
        case consoles::ListenerEvent::Imported: return PairStep::Imported;
        case consoles::ListenerEvent::BadCode: return PairStep::BadCode;
        case consoles::ListenerEvent::LockedOut: return PairStep::LockedOut;
        case consoles::ListenerEvent::TimedOut: return PairStep::TimedOut;
        case consoles::ListenerEvent::Error: break;
    }
    return PairStep::Error;
}

auto ProfileDesk::openPairing(bool createProfile, Paired step) -> Pairing
{
    closePairing();
    paired = std::move(step);
    pairCreates = createProfile;
    pairPort = services.networkSettings.companionPort();
    pairAddress = consoles::PairListener::localIpv4();
    listener.emplace(services.core);
    advertiser.emplace(services.core.threading());
    advertiser->start(pairPort);
    return renewPairing();
}

auto ProfileDesk::renewPairing() -> Pairing
{
    if (!listener)
        return {};
    const std::string code = listener->generateCode();
    listener->start(
        pairPort, code,
        [this, guard = alive](consoles::ListenerEvent event) {
            services.dispatcher.post([this, guard, event] {
                if (*guard && paired)
                    paired(pairStep(event));
            });
        },
        [this, guard = alive](const consoles::PairedCredentials& credentials) {
            services.dispatcher.post([this, guard, credentials] {
                if (*guard)
                    applyCredentials(credentials, pairCreates);
            });
        });
    return Pairing{ .code = code, .address = pairAddress, .port = pairPort };
}

void ProfileDesk::closePairing()
{
    if (advertiser)
        advertiser->stop();
    if (listener)
        listener->stop();
    advertiser.reset();
    listener.reset();
    paired = {};
}

void ProfileDesk::applyCredentials(const consoles::PairedCredentials& credentials, bool createProfile)
{
    log::info("[ACCOUNTS] pair push: create={} active={} onlineId={} accountId={} npsso={} refresh={} mobile={}",
        createProfile, services.accounts.activeProfileId(), !credentials.onlineId.empty(), !credentials.accountId.empty(),
        !credentials.npsso.empty(), !credentials.refreshToken.empty(), credentials.hasMobile);
    if (createProfile || !services.accounts.activeProfile())
        services.accounts.setActiveProfileId(services.accounts.addProfile(accounts::Profile{}));
    const accounts::Profile* profile = services.accounts.activeProfile();
    if (profile && profile->legacy)
    {
        log::warning("[ACCOUNTS] rejecting the pair: profile {} is legacy", profile->id);
        notifier.notify(services.strings.text("akira/pair/mode_mismatch"));
        return;
    }
    if (profile && !credentials.accountId.empty() && !profile->accountId.empty() && profile->accountId != credentials.accountId)
    {
        log::warning("[ACCOUNTS] the pair changes the account id on profile {}", profile->id);
        notifier.notify(services.strings.text("akira/pair/account_changed"));
    }
    if (!credentials.onlineId.empty())
        services.accounts.setPsnOnlineId(credentials.onlineId);
    if (!credentials.accountId.empty())
        services.accounts.setPsnAccountId(credentials.accountId);
    services.accounts.refreshLegacyGate();
    services.store.save();
    accounts::Accounts* psn = services.core.accounts();
    if (!psn || credentials.npsso.empty())
    {
        log::warning("[ACCOUNTS] the pair push carried no usable NPSSO");
        notifier.notify(services.strings.format("akira/hosts/token_refresh_failed", "NPSSO"));
        return;
    }
    const std::int64_t profileId = services.accounts.activeProfileId();
    psn->signIn(profileId, credentials.npsso, credentials.duid,
        [this, guard = alive, profileId](accounts::SignInOutcome outcome, const accounts::Accounts::User& user) {
            if (!*guard)
                return;
            if (outcome != accounts::SignInOutcome::SignedIn || !user)
            {
                log::warning("[ACCOUNTS] pair sign-in for profile {} ended without a user", profileId);
                if (outcome != accounts::SignInOutcome::Superseded)
                    notifier.notify(services.strings.format("akira/hosts/token_refresh_failed", "PSN"));
                return;
            }
            adopt(profileId, user);
            notifier.notify(services.strings.format("akira/qr_sign_in/signed_in_fmt", user->online_id()));
            log::info("[ACCOUNTS] signed in to PSN from the pairing push");
        });
}

void ProfileDesk::adopt(std::int64_t profileId, const accounts::Accounts::User& user)
{
    services.accounts.updateProfile(profileId, [&user](accounts::Profile& profile) {
        profile.accountId = accounts::Accounts::base64Of(user->account_id());
        profile.onlineId = user->online_id();
    });
    services.accounts.refreshLegacyGate();
    services.store.save();
    services.trophies.onActiveProfileChanged();
#if AKIRA_CLOUD
    services.cloud.clearCatalogCache();
    services.cloud.markActiveProfileDirty();
    services.cloud.refreshActiveProfile(true);
#endif
    if (profileChanged)
        profileChanged();
}

}
