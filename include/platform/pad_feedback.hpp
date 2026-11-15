#pragma once

#include <libchiaking/play.hpp>

namespace akira::platform
{

class PadFeedback
{
public:
    virtual ~PadFeedback() = default;

    virtual void apply(const libchiaking::controllers::ControllerFeedback& feedback) = 0;
};

}
