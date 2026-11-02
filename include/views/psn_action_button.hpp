#ifndef AKIRA_PSN_ACTION_BUTTON_HPP
#define AKIRA_PSN_ACTION_BUTTON_HPP

#include <borealis.hpp>

#include <functional>
#include <string>

#include "psn/accounts.hpp"

class PsnActionButton {
public:
    using StatusProvider = std::function<psn::ActionStatus()>;

    void attach(brls::Button* button, NVGcolor readyColor, std::string readyLabel,
                std::string busyLabel, std::string waitLabelKey, StatusProvider provider);

    bool isReady() const;

    static std::string formatWait(int seconds);

    void start();
    void stop();
    void apply();

private:
    brls::Button* button = nullptr;
    NVGcolor readyColor{};
    std::string readyLabel;
    std::string busyLabel;
    std::string waitLabelKey;
    StatusProvider provider;

    brls::RepeatingTimer timer;
    psn::ActionState appliedState = psn::ActionState::Ready;
    int appliedSeconds = -1;
    bool applied = false;
};

#endif // AKIRA_PSN_ACTION_BUTTON_HPP
