#include "accounts/ui/account_model.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <utility>

#include "log/log.hpp"

namespace akira::accounts::ui {

AccountModel::AccountModel(Parts parts)
    : held(std::move(parts))
{
}

AccountModel::~AccountModel()
{
    *alive = false;
    stopFlowWork();
}

auto AccountModel::countdown(int seconds) -> std::string
{
    const int shown = std::max(seconds, 0);
    return std::format("{}:{:02}", shown / 60, shown % 60);
}

auto AccountModel::secretKey(AccountDesk::Secret secret) -> const char*
{
    switch (secret)
    {
        case AccountDesk::Secret::AccessToken: return "akira/settings/access_token";
        case AccountDesk::Secret::RefreshToken: return "akira/settings/refresh_token";
        case AccountDesk::Secret::TokenExpires: return "akira/settings/token_expires";
        case AccountDesk::Secret::Npsso: return "akira/settings/npsso";
        case AccountDesk::Secret::SsoAccessToken: return "akira/settings/sso_access_token";
        case AccountDesk::Secret::SsoRefreshToken: return "akira/settings/sso_refresh_token";
        case AccountDesk::Secret::SsoExpires: return "akira/settings/sso_expires";
        case AccountDesk::Secret::Duid: break;
    }
    return "akira/settings/duid";
}

auto AccountModel::pairStatusKey(AccountDesk::PairStep step) -> const char*
{
    switch (step)
    {
        case AccountDesk::PairStep::Waiting: return "akira/pair/status_waiting";
        case AccountDesk::PairStep::Connecting: return "akira/pair/status_connecting";
        case AccountDesk::PairStep::Imported: return "akira/pair/status_imported";
        case AccountDesk::PairStep::BadCode: return "akira/pair/status_bad_code";
        case AccountDesk::PairStep::LockedOut: return "akira/pair/status_locked";
        case AccountDesk::PairStep::TimedOut: return "akira/pair/status_timed_out";
        case AccountDesk::PairStep::Error: break;
    }
    return "akira/pair/status_error";
}

auto AccountModel::qrStatusKey(AccountDesk::QrStep step) -> const char*
{
    switch (step)
    {
        case AccountDesk::QrStep::Preparing: return "akira/qr_sign_in/status_preparing";
        case AccountDesk::QrStep::Waiting: return "akira/qr_sign_in/status_waiting";
        case AccountDesk::QrStep::SigningIn:
        case AccountDesk::QrStep::SignedIn: return "akira/qr_sign_in/status_signing_in";
        case AccountDesk::QrStep::Failed: break;
    }
    return "akira/qr_sign_in/status_failed";
}

auto AccountModel::removalKey(std::size_t profiles) -> const char*
{
    return profiles <= 1 ? "akira/settings/remove_last_profile" : "akira/settings/remove_profile_named";
}

void AccountModel::reload()
{
    View next = state.get();
    next.entries = held.desk.entries();
    next.identity = held.desk.identity(next.revealed);
    next.credentials = held.desk.credentials(next.revealed);
    next.refresh = held.desk.refreshState();
    if (next.entries.empty())
        next.summary.reset();
    state.set(std::move(next));
    if (state.get().entries.empty())
        return;
    held.desk.summary([this, guard = alive](AccountDesk::Summary shown) {
        if (*guard)
            state.write(&View::summary, std::optional<AccountDesk::Summary>(std::move(shown)));
    });
}

void AccountModel::tick(Clock::time_point now)
{
    View next = state.get();
    next.refresh = held.desk.refreshState();
    if (next.flow == Flow::Qr)
    {
        next.qr.secondsLeft = -1;
        if (qrDeadline)
            next.qr.secondsLeft = static_cast<int>(std::max<std::int64_t>(0,
                std::chrono::duration_cast<std::chrono::seconds>(*qrDeadline - std::chrono::system_clock::now()).count()));
    }
    if (next.flow == Flow::Companion)
    {
        next.companion.secondsLeft = -1;
        if (pairWindow)
            next.companion.secondsLeft = static_cast<int>(std::max<std::int64_t>(0,
                AccountDesk::pairWindowSeconds - std::chrono::duration_cast<std::chrono::seconds>(now - *pairWindow).count()));
    }
    state.set(std::move(next));
}

void AccountModel::welcome(bool creating, bool firstRun)
{
    stopFlowWork();
    state.update([creating, firstRun](View& view) {
        view.flow = Flow::Welcome;
        view.creating = creating;
        view.firstRun = firstRun;
    });
}

void AccountModel::openIfEmpty()
{
    if (held.desk.entries().empty())
        welcome(false, true);
}

void AccountModel::stopFlowWork()
{
    switch (state.get().flow)
    {
        case Flow::Qr: held.desk.cancelSignIn(); break;
        case Flow::Companion: held.desk.closePairing(); break;
        case Flow::None:
        case Flow::Welcome:
        case Flow::Legacy: break;
    }
    qrDeadline.reset();
    pairWindow.reset();
}

void AccountModel::startQr()
{
    stopFlowWork();
    state.update([](View& view) {
        view.flow = Flow::Qr;
        view.qr = Qr{};
    });
    held.desk.signInWithQr(state.get().creating, [this, guard = alive](AccountDesk::QrProgress progress) {
        if (*guard && state.get().flow == Flow::Qr)
            qrProgress(progress);
    });
}

void AccountModel::retryQr()
{
    startQr();
}

void AccountModel::qrProgress(const AccountDesk::QrProgress& progress)
{
    switch (progress.step)
    {
        case AccountDesk::QrStep::Preparing:
            qrDeadline.reset();
            state.write(&View::qr, Qr{});
            return;
        case AccountDesk::QrStep::Waiting:
            qrDeadline = progress.expires;
            state.write(&View::qr, Qr{ .step = progress.step, .code = app::QrMatrix::encode(progress.url) });
            tick(Clock::now());
            return;
        case AccountDesk::QrStep::SigningIn:
            qrDeadline.reset();
            state.write(&View::qr, Qr{ .step = progress.step });
            return;
        case AccountDesk::QrStep::SignedIn:
            log::info("[ACCOUNTS] signed in to PSN with a QR code");
            qrDeadline.reset();
            held.notifier.notify(held.strings.format("akira/qr_sign_in/signed_in_fmt", progress.onlineId));
            state.write(&View::flow, Flow::None);
            reload();
            return;
        case AccountDesk::QrStep::Failed:
            qrDeadline.reset();
            state.write(&View::qr, Qr{ .step = progress.step, .failureKey = progress.failureKey });
            return;
    }
}

void AccountModel::restartPairingWindow()
{
    pairWindow = Clock::now();
    tick(*pairWindow);
}

void AccountModel::startCompanion()
{
    stopFlowWork();
    state.write(&View::flow, Flow::Companion);
    const AccountDesk::Pairing pairing = held.desk.openPairing(state.get().creating,
        [this, guard = alive](AccountDesk::PairStep step) {
            if (*guard && state.get().flow == Flow::Companion)
                pairStep(step);
        });
    state.write(&View::companion, Companion{ .pairing = pairing });
    restartPairingWindow();
}

void AccountModel::pairStep(AccountDesk::PairStep step)
{
    switch (step)
    {
        case AccountDesk::PairStep::Imported:
            log::info("[ACCOUNTS] companion credentials imported");
            closeFlow();
            return;
        case AccountDesk::PairStep::TimedOut:
            state.write(&View::companion, Companion{ .pairing = held.desk.renewPairing() });
            restartPairingWindow();
            return;
        case AccountDesk::PairStep::LockedOut:
            pairWindow.reset();
            state.update([step](View& view) {
                view.companion.step = step;
                view.companion.secondsLeft = -1;
            });
            return;
        case AccountDesk::PairStep::Waiting:
        case AccountDesk::PairStep::Connecting:
        case AccountDesk::PairStep::BadCode:
        case AccountDesk::PairStep::Error: break;
    }
    state.update([step](View& view) { view.companion.step = step; });
}

void AccountModel::startLegacy()
{
    stopFlowWork();
    state.update([](View& view) {
        view.flow = Flow::Legacy;
        view.legacyRejected = false;
    });
}

void AccountModel::back()
{
    const View& shown = state.get();
    if (shown.flow == Flow::Qr || shown.flow == Flow::Companion || shown.flow == Flow::Legacy)
    {
        if (shown.creating || shown.firstRun || held.desk.entries().empty())
        {
            welcome(shown.creating, shown.firstRun);
            return;
        }
    }
    closeFlow();
}

void AccountModel::closeFlow()
{
    stopFlowWork();
    state.update([](View& view) {
        view.flow = Flow::None;
        view.creating = false;
        view.firstRun = false;
    });
    reload();
}

void AccountModel::switchTo(std::int64_t id)
{
    const auto found = std::ranges::find(state.get().entries, id, &AccountDesk::Entry::id);
    if (found == state.get().entries.end() || found->active)
        return;
    held.desk.switchTo(id);
    held.notifier.notify(held.strings.text("akira/settings/profile_switched"));
    reload();
    if (!held.desk.signInExpired())
        return;
    log::info("[ACCOUNTS] the PSN sign-in for profile {} has expired; asking for a new one", id);
    state.update([](View& view) {
        view.creating = false;
        view.firstRun = false;
    });
    startQr();
}

void AccountModel::askRemove(std::int64_t id)
{
    state.write(&View::removing, std::optional<std::int64_t>(id));
}

void AccountModel::dismissRemove()
{
    state.write(&View::removing, std::optional<std::int64_t>());
}

void AccountModel::confirmRemove()
{
    const std::optional<std::int64_t> id = state.get().removing;
    dismissRemove();
    if (!id)
        return;
    held.desk.remove(*id);
    held.notifier.notify(held.strings.text("akira/settings/profile_removed"));
    reload();
    if (!state.get().entries.empty())
        return;
    if (held.emptied)
        held.emptied();
    welcome(false, true);
}

void AccountModel::setAccountId(const std::string& text)
{
    if (text == state.get().identity.accountId)
        return;
    if (!held.desk.setAccountId(text))
        held.notifier.notify(held.strings.text("akira/settings/psn_account_id_invalid"));
    reload();
}

void AccountModel::setOnlineId(const std::string& text)
{
    if (text == state.get().identity.onlineId)
        return;
    held.desk.setOnlineId(text);
    reload();
}

void AccountModel::setTrophies(bool enabled)
{
    held.desk.setTrophies(enabled);
    reload();
}

void AccountModel::setCompanionPort(const std::string& text)
{
    int port = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), port);
    if (error != std::errc{} || end != text.data() + text.size() || !held.desk.setCompanionPort(port))
        held.notifier.notify(held.strings.text("akira/settings/invalid_port"));
    reload();
}

void AccountModel::reveal(bool shown)
{
    state.write(&View::revealed, shown);
    reload();
}

void AccountModel::refreshToken()
{
    const AccountDesk::Refresh status = held.desk.refreshState();
    if (status.state == AccountDesk::RefreshState::NotLinked)
    {
        held.notifier.notify(held.strings.text("akira/settings/no_refresh_token"));
        return;
    }
    if (status.state != AccountDesk::RefreshState::Ready)
        return;
    held.notifier.notify(held.strings.text("akira/settings/refreshing_token"));
    held.desk.refresh([this, guard = alive](bool) {
        if (!*guard)
            return;
        held.notifier.notify(held.strings.text("akira/settings/token_refreshed"));
        reload();
    });
    state.write(&View::refresh, held.desk.refreshState());
}

void AccountModel::askClear()
{
    state.write(&View::clearing, true);
}

void AccountModel::dismissClear()
{
    state.write(&View::clearing, false);
}

void AccountModel::confirmClear()
{
    dismissClear();
    held.desk.clearPsn();
    held.notifier.notify(held.strings.text("akira/settings/psn_data_cleared"));
    reload();
}

void AccountModel::createLegacy(const std::string& accountId, const std::string& name)
{
    if (!held.desk.createLegacy(accountId, name))
    {
        state.write(&View::legacyRejected, true);
        return;
    }
    held.notifier.notify(held.strings.text("akira/profile_setup/created"));
    closeFlow();
}

}
