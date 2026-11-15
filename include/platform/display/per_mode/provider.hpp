#pragma once

#include "platform/display/provider.hpp"

namespace akira::platform::per_mode
{

class PerModeDisplayProvider final : public DisplayProvider
{
public:
    void prepare() override;
};

}
