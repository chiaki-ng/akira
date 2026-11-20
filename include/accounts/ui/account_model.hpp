#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <arisa/composition/observable.hpp>

#include "accounts/account_desk.hpp"
#include "app/notifier.hpp"
#include "app/qr_matrix.hpp"
#include "app/strings.hpp"

namespace akira::accounts::ui {

class AccountModel {
public:
    using Clock = std::chrono::steady_clock;

    enum class Flow { None, Welcome, Qr, Companion, Legacy };

    struct Qr {
        AccountDesk::QrStep step = AccountDesk::QrStep::Preparing;
        std::optional<app::QrMatrix> code;
        int secondsLeft = -1;
        std::string failureKey;

        friend bool operator==(const Qr&, const Qr&) = default;
    };

    struct Companion {
        AccountDesk::Pairing pairing;
        AccountDesk::PairStep step = AccountDesk::PairStep::Waiting;
        int secondsLeft = -1;

        friend bool operator==(const Companion&, const Companion&) = default;
    };

    struct View {
        std::vector<AccountDesk::Entry> entries;
        AccountDesk::Identity identity;
        std::vector<AccountDesk::Credential> credentials;
        bool revealed = false;
        std::optional<AccountDesk::Summary> summary;
        AccountDesk::Refresh refresh;
        Flow flow = Flow::None;
        bool creating = false;
        bool firstRun = false;
        Qr qr;
        Companion companion;
        bool legacyRejected = false;
        std::optional<std::int64_t> removing;
        bool clearing = false;

        friend bool operator==(const View&, const View&) = default;
    };

    struct Parts {
        AccountDesk& desk;
        app::Notifier& notifier;
        const app::Strings& strings;
        std::function<void()> emptied;
    };

    explicit AccountModel(Parts parts);
    ~AccountModel();

    AccountModel(const AccountModel&) = delete;
    AccountModel& operator=(const AccountModel&) = delete;

    auto view() const -> const ::arisa::composition::Observable<View>& { return state; }

    void reload();
    void tick(Clock::time_point now);

    void welcome(bool creating, bool firstRun = false);
    void openIfEmpty();
    void startQr();
    void retryQr();
    void startCompanion();
    void startLegacy();
    void back();
    void closeFlow();

    void switchTo(std::int64_t id);
    void askRemove(std::int64_t id);
    void confirmRemove();
    void dismissRemove();

    void setAccountId(const std::string& text);
    void setOnlineId(const std::string& text);
    void setTrophies(bool enabled);
    void setCompanionPort(const std::string& text);

    void reveal(bool shown);
    void refreshToken();
    void askClear();
    void confirmClear();
    void dismissClear();

    void createLegacy(const std::string& accountId, const std::string& name);

    static auto countdown(int seconds) -> std::string;
    static auto secretKey(AccountDesk::Secret secret) -> const char*;
    static auto pairStatusKey(AccountDesk::PairStep step) -> const char*;
    static auto qrStatusKey(AccountDesk::QrStep step) -> const char*;
    static auto removalKey(std::size_t profiles) -> const char*;

private:
    void stopFlowWork();
    void qrProgress(const AccountDesk::QrProgress& progress);
    void pairStep(AccountDesk::PairStep step);
    void restartPairingWindow();

    Parts held;
    ::arisa::composition::Observable<View> state;
    std::optional<std::chrono::system_clock::time_point> qrDeadline;
    std::optional<Clock::time_point> pairWindow;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

}
