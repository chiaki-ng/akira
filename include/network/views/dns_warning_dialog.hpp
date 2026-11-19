#pragma once

#include <chrono>
#include <memory>
#include <string>

#include <borealis.hpp>

namespace akira::network::views {

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
