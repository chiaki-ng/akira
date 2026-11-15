#pragma once

#include <libchiaking/play.hpp>

namespace akira::platform
{

class FeedbackProvider
{
public:
    virtual ~FeedbackProvider() = default;

    virtual void apply(const libchiaking::controllers::ControllerFeedback& feedback) = 0;
};

}
