#include "util/worker_cores.hpp"

extern "C" Result __real_svcSetThreadCoreMask(Handle handle, s32 preferred_core, u32 affinity_mask);

extern "C" Result __wrap_svcSetThreadCoreMask(Handle handle, s32 preferred_core, u32 affinity_mask)
{
    if (preferred_core < 0 || akira::WorkerCores::contains(preferred_core))
        affinity_mask = akira::WorkerCores::restrict(affinity_mask);
    return __real_svcSetThreadCoreMask(handle, preferred_core, affinity_mask);
}

namespace akira {

bool WorkerCores::contains(s32 core)
{
    return core >= 0 && core < 32 && (mask & (1u << core)) != 0;
}

Result WorkerCores::placeMainThread()
{
    if (!contains(mainCore))
        return 0;
    return svcSetThreadCoreMask(CUR_THREAD_HANDLE, mainCore, mask);
}

u32 WorkerCores::restrict(u32 affinityMask)
{
    const u32 restricted = affinityMask & mask;
    return restricted != 0 ? restricted : affinityMask;
}

}
