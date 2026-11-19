#pragma once

#include <chrono>
#include <memory>
#include <string>

#include <borealis.hpp>

namespace akira::network::views {

// Borealis dismisses a Dialog before invoking its button callback. Disable
// the actual A/touch action until the deadline, not merely the button style.
class DnsWarningDialog final : public brls::Dialog {
public:
    explicit DnsWarningDialog(std::string message);
    ~DnsWarningDialog() override;

    void show();

    static void checkAtStartup();

private:
    struct LiveState {
        DnsWarningDialog* dialog = nullptr;
    };

    int remainingSeconds() const;
    void updateCountdown();

    std::shared_ptr<LiveState> liveState = std::make_shared<LiveState>();
    std::chrono::steady_clock::time_point openedAt;
    int lastRemaining = -1;
};

}
