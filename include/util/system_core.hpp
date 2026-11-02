#ifndef AKIRA_SYSTEM_CORE_HPP
#define AKIRA_SYSTEM_CORE_HPP

#include <switch.h>

namespace akira {

class SystemCore {
public:
    static constexpr s32 index = 3;

    static u32 withoutSystemCore(u32 affinityMask);
};

}

#endif
