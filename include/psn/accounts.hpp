#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <libchiaking/chiaki_ng.hpp>

#include "core/background_work.hpp"

class SettingsManager;

namespace psn {

enum class ActionState {
    Ready,
    Busy,
    CoolingDown
};

struct ActionStatus {
    ActionState state = ActionState::Ready;
    int secondsRemaining = 0;
};

enum class SessionState {
    NotLinked,
    Valid,
    Expired,
    Refreshing
};

enum class SignInOutcome {
    SignedIn,
    Rejected,
    Offline,
    Failed,
    Superseded
};

class Accounts {
public:
    using User = std::shared_ptr<libchiaking::users::PsUser>;
    using StateObserver = std::function<void()>;
    using SignedInCallback = std::function<void(SignInOutcome, const User&)>;
    using RefreshedCallback = std::function<void(bool)>;
    using RenewedObserver = std::function<void()>;
    using QrProgress = std::function<void(const libchiaking::users::QrSignInState&)>;

    Accounts(libchiaking::ChiakiNg& chiaki, SettingsManager& settings,
             libchiaking::platform::ThreadingProvider& threading, libchiaking::platform::ClockProvider& clock,
             libchiaking::platform::SecureRandomProvider& random);
    ~Accounts();

    Accounts(const Accounts&) = delete;
    Accounts& operator=(const Accounts&) = delete;

    auto activeUser() -> User;
    auto state() -> SessionState;
    auto refreshStatus() const -> ActionStatus;
    void setStateObserver(StateObserver observer);
    void setRenewedObserver(RenewedObserver observer);

    void signIn(int64_t profileId, std::string_view npsso, std::string_view deviceId, SignedInCallback done);
    void signInWithQr(QrProgress progress, SignedInCallback done);
    void cancelSignIn();
    void refreshNow(RefreshedCallback done);
    [[nodiscard]] auto staySignedIn(const libchiaking::platform::StopToken& stop) -> bool;
    void forget(std::string_view base64Account);

    static auto base64Of(libchiaking::AccountId account) -> std::string;

private:
    class Shared;

    static constexpr std::chrono::seconds keepAlivePeriod{300};
    static constexpr std::chrono::seconds refreshCooldown{60};
    static constexpr std::chrono::seconds failedCooldown{5};

    using Delivery = std::function<void(SignInOutcome, User)>;

    auto beginSignIn(SignedInCallback done) -> std::pair<uint64_t, Delivery>;
    void startSignInThread(const char* name, std::function<void(libchiaking::platform::StopToken)> work,
                           const Delivery& deliver);
    static auto outcomeOf(const libchiaking::users::SignInError& error) -> SignInOutcome;
    void keepAlive(const libchiaking::platform::StopToken& stop);
    auto keepEveryoneSignedIn(const libchiaking::platform::StopToken& stop) -> bool;
    auto correlationId() -> std::string;
    void notifyStateChanged();
    void followBackgroundWork(bool paused);

    libchiaking::ChiakiNg& chiaki;
    SettingsManager& settings;
    libchiaking::platform::ThreadingProvider& threading;
    libchiaking::platform::ClockProvider& clock;
    libchiaking::platform::SecureRandomProvider& random;
    std::shared_ptr<Shared> shared;
    libchiaking::platform::Thread signInThread;
    libchiaking::platform::Thread keepAliveThread;
    akira::BackgroundWork::Subscription backgroundSubscription;
};

}
