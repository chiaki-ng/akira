#include "app/arisa/thread_plan.hpp"

namespace akira::app::arisa {

namespace provider = ::arisa::provider;

auto ThreadPlan::placement(std::uint64_t cores, provider::ThreadPriority priority) -> provider::ThreadPlacement
{
    return provider::ThreadPlacement::create(provider::CoreMask{cores}, priority,
        provider::StackSize{provider::ThreadPlacementPolicy::thread_stack})
        .value();
}

auto ThreadPlan::policy() -> provider::ThreadPlacementPolicy
{
#if defined(__SWITCH2__)
    constexpr Cores cores = ounce;
#else
    constexpr Cores cores = nx;
#endif
    return provider::ThreadPlacementPolicy{}
        .with(provider::ThreadRole::ui, placement(cores.ui, provider::ThreadPriority::of<0x2C>()))
        .with(provider::ThreadRole::render, placement(cores.render, provider::ThreadPriority::of<0x2B>()))
        .with(provider::ThreadRole::background, placement(cores.workers, provider::ThreadPriority::of<0x2D>()))
        .with(provider::ThreadRole::blocking, placement(cores.blocking, provider::ThreadPriority::of<0x2E>()))
        .with(provider::ThreadRole::secondary_root, placement(cores.ui, provider::ThreadPriority::of<0x2C>()))
        .with(provider::ThreadRole::app, placement(cores.workers, provider::ThreadPriority::of<0x2C>()));
}

}
