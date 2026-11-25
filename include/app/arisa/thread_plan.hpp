#pragma once

#include <cstdint>

#include <arisa/provider/threading/thread_placement_policy.hpp>

namespace akira::app::arisa {

class ThreadPlan {
public:
    static auto policy() -> ::arisa::provider::ThreadPlacementPolicy;

private:
    struct Cores {
        std::uint64_t ui;
        std::uint64_t render;
        std::uint64_t workers;
        std::uint64_t blocking;
    };

    static constexpr Cores nx{ .ui = 0b0100, .render = 0b0100, .workers = 0b0110, .blocking = 0b0100 };
    static constexpr Cores ounce{ .ui = 0b010000, .render = 0b001000, .workers = 0b101000, .blocking = 0b100000 };

    static auto placement(std::uint64_t cores, ::arisa::provider::ThreadPriority priority)
        -> ::arisa::provider::ThreadPlacement;
};

}
