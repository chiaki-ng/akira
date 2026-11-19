#pragma once

#include <libchiaking/platform/platform.hpp>
#include <libchiaking/play_types.hpp>

namespace akira::play {
class ChiakiCore;
}

namespace akira::network {

class NatProbe {
public:
    using Result = libchiaking::Expected<libchiaking::play::NatType, libchiaking::play::NatTypeFailure>;

    explicit NatProbe(play::ChiakiCore& core);

    [[nodiscard]] Result check(const libchiaking::platform::StopToken& stop = {});

private:
    play::ChiakiCore& core;
};

}
