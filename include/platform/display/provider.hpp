#pragma once

namespace akira::platform
{

class DisplayProvider
{
public:
    virtual ~DisplayProvider() = default;

    virtual void prepare() = 0;
};

}
