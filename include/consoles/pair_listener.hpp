#ifndef AKIRA_PAIR_LISTENER_HPP
#define AKIRA_PAIR_LISTENER_HPP

#include <cstdint>
#include <functional>
#include <string>

#include <libchiaking/platform/platform.hpp>

namespace akira::pair {

enum class ListenerEvent {
    Listening,
    ClientConnected,
    Imported,
    BadCode,
    LockedOut,
    TimedOut,
    Error,
};

struct PairedCredentials {
    std::string duid;
    std::string accountId;
    std::string onlineId;
    std::string accessToken;
    std::string refreshToken;
    int expiresIn = 0;
    int64_t expiresAt = 0;
    bool isExpired = false;
    std::string npsso;
    bool hasMobile = false;
    std::string mobileAccessToken;
    std::string mobileRefreshToken;
    int64_t mobileExpiresAt = 0;
};

class PairListener {
public:
    using EventCallback = std::function<void(ListenerEvent)>;
    using ImportCallback = std::function<void(const PairedCredentials&)>;

    PairListener() = default;
    ~PairListener();

    void start(int port, const std::string& code, EventCallback onEvent, ImportCallback onImport);
    void stop();

    static std::string generateCode();
    static std::string localIpv4();

    static constexpr int kMaxTries = 3;
    static constexpr int kWindowSeconds = 60;

private:
    void run(int port, const std::string& code, const libchiaking::platform::StopToken& stop);

    libchiaking::platform::Thread thread_;
    EventCallback onEvent_;
    ImportCallback onImport_;
};

}

#endif
