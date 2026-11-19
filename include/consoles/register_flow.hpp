#pragma once

#include <atomic>
#include <optional>

#include "consoles/host.hpp"

namespace akira::consoles {

class RegisterFlow {
public:
    enum class Stage { Account, Console, Connect, Finish };
    enum class Notice { Success, Canceled, Failed };
    enum class Refusal { AccountId, OnlineId, ConsoleType, Failed };
    enum class InternetEnd { Success, Quiet, FallBackToPin };

    static Stage stage(int index);

    bool registering() const { return m_registering; }

    void beginPin();
    void stopped();
    Notice pinEnded(Host::PairingOutcome outcome);
    std::optional<Refusal> pinRefused(HostRegisterError result);

    bool beginInternet();
    [[nodiscard]] bool internetStarted(HostRegisterError result);
    void userCanceled();
    InternetEnd internetEnded(Host::PairingOutcome outcome);

private:
    bool m_registering = false;
    std::atomic<bool> m_canceled{false};
};

}
