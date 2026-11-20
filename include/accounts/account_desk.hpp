#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace akira::accounts {

class AccountDesk {
public:
    enum class Kind { Remote, Local, Legacy };

    struct Entry {
        std::int64_t id = 0;
        std::string name;
        Kind kind = Kind::Local;
        bool active = false;

        friend bool operator==(const Entry&, const Entry&) = default;
    };

    struct Identity {
        std::string accountId;
        std::string onlineId;
        bool legacy = false;
        bool trophies = true;
        int companionPort = 0;

        friend bool operator==(const Identity&, const Identity&) = default;
    };

    enum class Secret { AccessToken, RefreshToken, TokenExpires, Npsso, SsoAccessToken, SsoRefreshToken, SsoExpires, Duid };

    struct Credential {
        Secret secret = Secret::AccessToken;
        std::optional<std::string> shown;
        bool expired = false;

        friend bool operator==(const Credential&, const Credential&) = default;
    };

    struct Trophies {
        int level = 0;
        int progress = 0;
        int platinum = 0;
        int gold = 0;
        int silver = 0;
        int bronze = 0;

        friend bool operator==(const Trophies&, const Trophies&) = default;
    };

    struct Summary {
        std::string name;
        Kind kind = Kind::Local;
        std::string cloudKey;
        bool plus = false;
        std::optional<Trophies> trophies;

        friend bool operator==(const Summary&, const Summary&) = default;
    };

    enum class RefreshState { Ready, Busy, CoolingDown, NotLinked };

    struct Refresh {
        RefreshState state = RefreshState::NotLinked;
        int seconds = 0;

        friend bool operator==(const Refresh&, const Refresh&) = default;
    };

    enum class QrStep { Preparing, Waiting, SigningIn, SignedIn, Failed };

    struct QrProgress {
        QrStep step = QrStep::Preparing;
        std::string url;
        std::optional<std::chrono::system_clock::time_point> expires;
        std::string failureKey;
        std::string onlineId;

        friend bool operator==(const QrProgress&, const QrProgress&) = default;
    };

    enum class PairStep { Waiting, Connecting, Imported, BadCode, LockedOut, TimedOut, Error };

    struct Pairing {
        std::string code;
        std::string address;
        int port = 0;

        friend bool operator==(const Pairing&, const Pairing&) = default;
    };

    using Qr = std::function<void(QrProgress)>;
    using Paired = std::function<void(PairStep)>;

    static constexpr int pairWindowSeconds = 60;

    virtual ~AccountDesk() = default;

    virtual auto entries() const -> std::vector<Entry> = 0;
    virtual auto identity(bool revealed) const -> Identity = 0;
    virtual auto credentials(bool revealed) const -> std::vector<Credential> = 0;
    virtual void summary(std::function<void(Summary)> shown) = 0;

    virtual void switchTo(std::int64_t id) = 0;
    virtual auto signInExpired() const -> bool = 0;
    virtual void remove(std::int64_t id) = 0;
    [[nodiscard]] virtual auto setAccountId(const std::string& text) -> bool = 0;
    virtual void setOnlineId(const std::string& text) = 0;
    virtual void setTrophies(bool enabled) = 0;
    [[nodiscard]] virtual auto setCompanionPort(int port) -> bool = 0;

    virtual auto refreshState() const -> Refresh = 0;
    virtual void refresh(std::function<void(bool)> done) = 0;
    virtual void clearPsn() = 0;

    [[nodiscard]] virtual auto createLegacy(const std::string& accountId, const std::string& name) -> bool = 0;
    virtual void signInWithQr(bool createProfile, Qr progress) = 0;
    virtual void cancelSignIn() = 0;
    virtual auto openPairing(bool createProfile, Paired step) -> Pairing = 0;
    virtual auto renewPairing() -> Pairing = 0;
    virtual void closePairing() = 0;
};

}
