#include "util/system_core.hpp"

extern "C" Result __real_svcSetThreadCoreMask(Handle handle, s32 preferred_core, u32 affinity_mask);

extern "C" Result __wrap_svcSetThreadCoreMask(Handle handle, s32 preferred_core, u32 affinity_mask)
{
    if (preferred_core != akira::SystemCore::index)
        affinity_mask = akira::SystemCore::withoutSystemCore(affinity_mask);
    return __real_svcSetThreadCoreMask(handle, preferred_core, affinity_mask);
}

namespace akira {

u32 SystemCore::withoutSystemCore(u32 affinityMask)
{
    const u32 stripped = affinityMask & ~(1u << index);
    return stripped != 0 ? stripped : affinityMask;
}

}
