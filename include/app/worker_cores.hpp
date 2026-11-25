#ifndef AKIRA_WORKER_CORES_HPP
#define AKIRA_WORKER_CORES_HPP

#include <switch.h>

namespace akira::app {

class WorkerCores {
public:
    static constexpr u32 mask = AKIRA_WORKER_CORE_MASK;
    static constexpr s32 mainCore = AKIRA_MAIN_CORE;

    static bool contains(s32 core);
    static u32 restrict(u32 affinityMask);
    static Result placeMainThread();
};

}

#endif
