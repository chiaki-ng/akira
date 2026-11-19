#pragma once

#include "platform/feedback/provider.hpp"

namespace akira::play {
class HapticManager;
}

namespace akira::platform::hid
{

class HidFeedbackProvider final : public FeedbackProvider
{
public:
    explicit HidFeedbackProvider(play::HapticManager& haptics);

    void apply(const libchiaking::controllers::ControllerFeedback& feedback) override;

private:
    play::HapticManager& m_haptics;
};

}
