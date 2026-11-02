#include "psn/accounts.hpp"
#include "core/settings_manager.hpp"
#include "util/base64.hpp"

#include <borealis.hpp>

#include <array>
#include <format>
#include <utility>
#include <vector>

namespace psn {

namespace platform = libchiaking::platform;
using libchiaking::users::DeviceId;
using libchiaking::users::SignInState;

class Accounts::Shared {
public:
    Shared(platform::ThreadingProvider& threading, const platform::ClockProvider& clock)
        : mutex(threading), wake(threading, clock)
    {
    }

    platform::Mutex mutex;
    platform::ConditionVariable wake;
    bool wakeRequested = false;
    bool refreshing = false;
    bool userRequested = false;
    bool paused = false;
    bool renewAfterPause = false;
    uint64_t readyAtMicroseconds = 0;
    uint64_t signInGeneration = 0;
    uint64_t refreshGeneration = 0;
    std::vector<std::pair<uint64_t, RefreshedCallback>> refreshWaiters;
    StateObserver observer;
    RenewedObserver renewed;
};

Accounts::Accounts(libchiaking::ChiakiNg& chiaki, SettingsManager& settings,
                   platform::ThreadingProvider& threading, platform::ClockProvider& clock,
                   platform::SecureRandomProvider& random)
    : chiaki(chiaki),
      settings(settings),
      threading(threading),
      clock(clock),
      random(random),
      shared(std::make_shared<Shared>(threading, clock))
{
    auto started = platform::Thread::start(threading, "psn-keep-alive", platform::ThreadRole::unspecified,
        [this](platform::StopToken stop) { keepAlive(stop); });
    if (started)
        keepAliveThread = std::move(started).value();
    else
        brls::Logger::error("PSN keep-alive thread could not start");
    backgroundSubscription = akira::BackgroundWork::subscribe([this](bool paused) { followBackgroundWork(paused); });
}

void Accounts::followBackgroundWork(bool paused)
{
    bool renew = false;
    {
        platform::Lock lock(shared->mutex);
        shared->paused = paused;
        if (!paused && std::exchange(shared->renewAfterPause, false)) {
            shared->wakeRequested = true;
            renew = true;
        }
    }
    if (renew) {
        brls::Logger::info("PSN keep-alive: renewing after the stream");
        shared->wake.notify_all();
    }
}

Accounts::~Accounts()
{
    {
        platform::Lock lock(shared->mutex);
        shared->signInGeneration++;
        shared->refreshWaiters.clear();
        shared->observer = nullptr;
        shared->renewed = nullptr;
    }
    static_cast<void>(signInThread.request_stop());
    static_cast<void>(keepAliveThread.request_stop());
    shared->wake.notify_all();
    signInThread = platform::Thread{};
    keepAliveThread = platform::Thread{};
}

auto Accounts::activeUser() -> User
{
    std::optional<libchiaking::AccountId> account;
    std::string name;
    bool legacy = false;
    settings.readProfile(settings.getActiveProfileId(), [&](const Profile& profile) {
        account = libchiaking::AccountId::from_base64(profile.accountId);
        name = profile.onlineId;
        legacy = profile.legacy;
    });
    if (!account)
        return nullptr;

    if (auto user = chiaki.users().find(*account))
        return user;
    if (!legacy || name.empty())
        return nullptr;

    auto added = chiaki.users().add_home_user(*account, name);
    if (!added) {
        brls::Logger::error("PSN: legacy profile {} could not be added as a home user", name);
        return nullptr;
    }
    return std::move(added).value();
}

auto Accounts::state() -> SessionState
{
    const User user = activeUser();
    if (!user || !user->has_psn_account())
        return SessionState::NotLinked;
    {
        platform::Lock lock(shared->mutex);
        if (shared->refreshing)
            return SessionState::Refreshing;
    }
    switch (user->sign_in_state()) {
        case SignInState::signed_in:
        case SignInState::store_access_lapsed:
            return SessionState::Valid;
        case SignInState::expired:
            return SessionState::Expired;
        case SignInState::signed_out:
            break;
    }
    return SessionState::NotLinked;
}

auto Accounts::refreshStatus() const -> ActionStatus
{
    platform::Lock lock(shared->mutex);
    if (shared->refreshing && !shared->refreshWaiters.empty())
        return ActionStatus{ActionState::Busy, 0};
    const uint64_t now = clock.now_microseconds();
    if (now < shared->readyAtMicroseconds) {
        const auto remaining = (shared->readyAtMicroseconds - now + 999'999) / 1'000'000;
        return ActionStatus{ActionState::CoolingDown, static_cast<int>(remaining)};
    }
    return ActionStatus{};
}

void Accounts::setStateObserver(StateObserver observer)
{
    platform::Lock lock(shared->mutex);
    shared->observer = std::move(observer);
}

void Accounts::setRenewedObserver(RenewedObserver observer)
{
    platform::Lock lock(shared->mutex);
    shared->renewed = std::move(observer);
}

void Accounts::signIn(int64_t profileId, std::string_view npsso, std::string_view deviceId, SignedInCallback done)
{
    auto deliver = beginSignIn(std::move(done)).second;

    const libchiaking::users::Npsso seed{npsso};
    if (seed.refusal()) {
        deliver(SignInOutcome::Rejected, nullptr);
        return;
    }

    auto device = DeviceId::parse(deviceId);
    if (!device)
        device = DeviceId::generate(random);
    if (!device) {
        deliver(SignInOutcome::Failed, nullptr);
        return;
    }

    brls::Logger::info("PSN: signing in profile {}", profileId);
    auto work = [this, seed = std::make_shared<libchiaking::users::Npsso>(seed.clone()),
                 device = std::make_shared<DeviceId>(std::move(*device)), correlation = correlationId(),
                 deliver](platform::StopToken stop) {
        auto signedIn = chiaki.users().sign_in(*seed, *device, correlation, stop);
        if (signedIn) {
            notifyStateChanged();
            deliver(SignInOutcome::SignedIn, std::move(signedIn).value());
            return;
        }
        const auto& error = signedIn.error();
        brls::Logger::warning("PSN: sign-in failed at {}: {} ({})", error.step(), error.name(), error.cause());
        deliver(outcomeOf(error), nullptr);
    };
    startSignInThread("psn-sign-in", std::move(work), deliver);
}

void Accounts::signInWithQr(QrProgress progress, SignedInCallback done)
{
    auto [generation, deliver] = beginSignIn(std::move(done));

    auto device = DeviceId::generate(random);
    if (!device) {
        deliver(SignInOutcome::Failed, nullptr);
        return;
    }

    auto report = [weak = std::weak_ptr<Shared>(shared), generation, progress = std::move(progress)](
                      const libchiaking::users::QrSignInState& state) {
        brls::sync([weak, generation, progress,
                    snapshot = std::make_shared<libchiaking::users::QrSignInState>(state.clone())] {
            const auto alive = weak.lock();
            if (!alive)
                return;
            {
                platform::Lock lock(alive->mutex);
                if (alive->signInGeneration != generation)
                    return;
            }
            progress(*snapshot);
        });
    };

    brls::Logger::info("PSN: signing in with a QR code");
    auto work = [this, device = std::make_shared<DeviceId>(std::move(*device)), correlation = correlationId(),
                 report = std::move(report), deliver](platform::StopToken stop) {
        auto signedIn = chiaki.users().sign_in_with_qr(*device, correlation, stop, report);
        if (signedIn) {
            notifyStateChanged();
            deliver(SignInOutcome::SignedIn, std::move(signedIn).value());
            return;
        }
        const auto& error = signedIn.error();
        brls::Logger::warning("PSN: QR sign-in failed at {}: {} ({})", error.step(), error.name(), error.cause());
        deliver(outcomeOf(error), nullptr);
    };
    startSignInThread("psn-qr-sign-in", std::move(work), deliver);
}

auto Accounts::beginSignIn(SignedInCallback done) -> std::pair<uint64_t, Delivery>
{
    uint64_t generation = 0;
    {
        platform::Lock lock(shared->mutex);
        generation = ++shared->signInGeneration;
    }
    signInThread = platform::Thread{};

    Delivery deliver = [weak = std::weak_ptr<Shared>(shared), generation, done = std::move(done)](
                           SignInOutcome outcome, User user) {
        brls::sync([weak, generation, done, outcome, user = std::move(user)] {
            const auto alive = weak.lock();
            if (!alive)
                return;
            {
                platform::Lock lock(alive->mutex);
                if (alive->signInGeneration != generation)
                    return;
            }
            done(outcome, user);
        });
    };
    return {generation, std::move(deliver)};
}

void Accounts::startSignInThread(const char* name, std::function<void(platform::StopToken)> work,
                                 const Delivery& deliver)
{
    auto started = platform::Thread::start(threading, name, platform::ThreadRole::unspecified, std::move(work));
    if (!started) {
        deliver(SignInOutcome::Failed, nullptr);
        return;
    }
    signInThread = std::move(started).value();
}

auto Accounts::outcomeOf(const libchiaking::users::SignInError& error) -> SignInOutcome
{
    switch (error.kind()) {
        case libchiaking::users::SignInErrorKind::rejected:
        case libchiaking::users::SignInErrorKind::declined:
            return SignInOutcome::Rejected;
        case libchiaking::users::SignInErrorKind::transport:
            return SignInOutcome::Offline;
        case libchiaking::users::SignInErrorKind::cancelled:
            return SignInOutcome::Superseded;
        default:
            return SignInOutcome::Failed;
    }
}

void Accounts::cancelSignIn()
{
    {
        platform::Lock lock(shared->mutex);
        shared->signInGeneration++;
    }
    static_cast<void>(signInThread.request_stop());
}

void Accounts::refreshNow(RefreshedCallback done)
{
    {
        platform::Lock lock(shared->mutex);
        shared->refreshWaiters.emplace_back(shared->refreshGeneration, std::move(done));
        shared->userRequested = true;
        shared->wakeRequested = true;
    }
    shared->wake.notify_all();
}

auto Accounts::staySignedIn(const platform::StopToken& stop) -> bool
{
    const User user = activeUser();
    if (!user)
        return false;
    if (!user->has_psn_account())
        return true;
    const auto renewed = user->stay_signed_in(stop);
    notifyStateChanged();
    return renewed && *renewed == SignInState::signed_in;
}

void Accounts::forget(std::string_view base64Account)
{
    const auto account = libchiaking::AccountId::from_base64(base64Account);
    if (!account)
        return;
    if (!chiaki.sign_out(*account))
        brls::Logger::warning("PSN: the saved sign-in could not be deleted");
    notifyStateChanged();
}

auto Accounts::base64Of(libchiaking::AccountId account) -> std::string
{
    const auto bytes = account.bytes();
    return akira::base64::encode(bytes);
}

void Accounts::keepAlive(const platform::StopToken& stop)
{
    brls::Logger::info("PSN keep-alive: started");
    bool first = true;
    while (!stop.stop_requested()) {
        if (!first) {
            platform::Lock lock(shared->mutex);
            static_cast<void>(shared->wake.wait_for(lock, keepAlivePeriod, stop,
                [this] { return shared->wakeRequested; }));
        }
        first = false;
        if (stop.stop_requested())
            break;
        {
            platform::Lock lock(shared->mutex);
            if (shared->paused) {
                shared->wakeRequested = false;
                shared->renewAfterPause = true;
                continue;
            }
        }

        std::vector<std::pair<uint64_t, RefreshedCallback>> waiters;
        bool userRequested = false;
        {
            platform::Lock lock(shared->mutex);
            shared->wakeRequested = false;
            shared->refreshing = true;
            userRequested = std::exchange(shared->userRequested, false);
            waiters = std::move(shared->refreshWaiters);
            shared->refreshWaiters.clear();
            shared->refreshGeneration++;
        }
        notifyStateChanged();

        const bool ok = keepEveryoneSignedIn(stop);

        {
            platform::Lock lock(shared->mutex);
            shared->refreshing = false;
            if (userRequested)
                shared->readyAtMicroseconds = clock.now_microseconds() +
                    static_cast<uint64_t>(std::chrono::microseconds(ok ? refreshCooldown : failedCooldown).count());
        }
        notifyStateChanged();

        for (auto& waiter : waiters) {
            brls::sync([weak = std::weak_ptr<Shared>(shared), done = std::move(waiter.second), ok] {
                if (weak.lock())
                    done(ok);
            });
        }

        if (ok) {
            brls::sync([weak = std::weak_ptr<Shared>(shared)] {
                RenewedObserver renewed;
                if (const auto alive = weak.lock()) {
                    platform::Lock lock(alive->mutex);
                    renewed = alive->renewed;
                }
                if (renewed)
                    renewed();
            });
        }
    }
    brls::Logger::info("PSN keep-alive: stopped");
}

auto Accounts::keepEveryoneSignedIn(const platform::StopToken& stop) -> bool
{
    if (settings.isLegacyProfileActive()) {
        brls::Logger::debug("PSN keep-alive: legacy profile active, nothing to renew");
        return true;
    }

    bool ok = true;
    for (const User& user : chiaki.users().everyone()) {
        if (stop.stop_requested())
            return false;
        if (!user->has_psn_account() || user->sign_in_state() == SignInState::signed_out)
            continue;
        if (user->sign_in_state() == SignInState::expired) {
            brls::Logger::debug("PSN keep-alive: {} needs a new sign-in, not retrying", user->online_id());
            continue;
        }
        const auto renewed = user->stay_signed_in(stop);
        if (!renewed) {
            brls::Logger::warning("PSN keep-alive: {} could not be renewed (reason {})", user->online_id(),
                static_cast<int>(renewed.error().reason));
            ok = false;
        } else if (*renewed != SignInState::signed_in) {
            brls::Logger::warning("PSN keep-alive: {} needs a new sign-in", user->online_id());
            ok = false;
        }
    }
    return ok;
}

auto Accounts::correlationId() -> std::string
{
    std::array<std::byte, 16> bytes{};
    if (!random.fill_bytes(bytes))
        return "00000000-0000-4000-8000-000000000000";
    bytes[6] = (bytes[6] & std::byte{0x0f}) | std::byte{0x40};
    bytes[8] = (bytes[8] & std::byte{0x3f}) | std::byte{0x80};
    std::string text;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            text.push_back('-');
        std::format_to(std::back_inserter(text), "{:02x}", std::to_integer<unsigned>(bytes[i]));
    }
    return text;
}

void Accounts::notifyStateChanged()
{
    StateObserver observer;
    {
        platform::Lock lock(shared->mutex);
        observer = shared->observer;
    }
    if (observer)
        observer();
}

}
