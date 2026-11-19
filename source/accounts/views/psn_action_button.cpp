#include "accounts/views/psn_action_button.hpp"
#include "app/views/theme.hpp"

#include <format>

static constexpr brls::Time PSN_ACTION_TICK_MS = 250;

void PsnActionButton::attach(brls::Button* button, NVGcolor readyColor, std::string readyLabel,
                             std::string busyLabel, std::string waitLabelKey, StatusProvider provider)
{
    this->button = button;
    this->readyColor = readyColor;
    this->readyLabel = std::move(readyLabel);
    this->busyLabel = std::move(busyLabel);
    this->waitLabelKey = std::move(waitLabelKey);
    this->provider = std::move(provider);

    timer.setCallback([this]() { apply(); });
    apply();
}

std::string PsnActionButton::formatWait(int seconds)
{
    if (seconds < 60)
        return std::format("{}s", seconds);

    if (seconds < 3600)
        return std::format("{}m", (seconds + 59) / 60);

    int hours = seconds / 3600;
    int minutes = ((seconds % 3600) + 59) / 60;
    if (minutes == 60)
    {
        hours++;
        minutes = 0;
    }

    return minutes > 0 ? std::format("{}h {}m", hours, minutes) : std::format("{}h", hours);
}

bool PsnActionButton::isReady() const
{
    return provider && provider().state == psn::ActionState::Ready;
}

void PsnActionButton::start()
{
    apply();
    timer.start(PSN_ACTION_TICK_MS);
}

void PsnActionButton::stop()
{
    timer.stop();
}

void PsnActionButton::apply()
{
    if (!button || !provider)
        return;

    psn::ActionStatus status = provider();

    if (applied && status.state == appliedState && status.secondsRemaining == appliedSeconds)
        return;

    applied = true;
    appliedState = status.state;
    appliedSeconds = status.secondsRemaining;

    switch (status.state)
    {
        case psn::ActionState::Ready:
            button->setText(readyLabel);
            button->setState(brls::ButtonState::ENABLED);
            button->setTextColor(brls::Application::getTheme()["brls/button/primary_enabled_text"]);
            button->setBackgroundColor(readyColor);
            break;

        case psn::ActionState::Busy:
            button->setText(busyLabel);
            button->setState(brls::ButtonState::DISABLED);
            button->setTextColor(akira::ui::active().textDim);
            button->setBackgroundColor(akira::ui::active().surface);
            break;

        case psn::ActionState::CoolingDown:
            button->setText(brls::getStr(waitLabelKey, formatWait(status.secondsRemaining)));
            button->setState(brls::ButtonState::DISABLED);
            button->setTextColor(akira::ui::active().textDim);
            button->setBackgroundColor(akira::ui::active().surface);
            break;
    }
}
