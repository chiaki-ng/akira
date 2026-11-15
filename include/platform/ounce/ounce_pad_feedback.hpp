#pragma once

#include "platform/pad_feedback.hpp"

class HapticManager;

namespace akira::platform
{

class OuncePadFeedback final : public PadFeedback
{
public:
    explicit OuncePadFeedback(HapticManager& haptics);

    void apply(const libchiaking::controllers::ControllerFeedback& feedback) override;

private:
    HapticManager& m_haptics;
};

}
