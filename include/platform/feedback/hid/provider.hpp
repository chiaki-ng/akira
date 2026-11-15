#pragma once

#include "platform/feedback/provider.hpp"

class HapticManager;

namespace akira::platform::hid
{

class HidFeedbackProvider final : public FeedbackProvider
{
public:
    explicit HidFeedbackProvider(HapticManager& haptics);

    void apply(const libchiaking::controllers::ControllerFeedback& feedback) override;

private:
    HapticManager& m_haptics;
};

}
