#pragma once

#include "platform/display/provider.hpp"

namespace akira::platform::fixed
{

class FixedDisplayProvider final : public DisplayProvider
{
public:
    void prepare() override {}
};

}
