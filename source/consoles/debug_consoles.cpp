#include "consoles/debug_consoles.hpp"

namespace akira::consoles {

std::vector<DebugConsoles::Console> DebugConsoles::list()
{
    std::vector<Console> consoles;
    for (int i = 1; i <= Count; i++) {
        consoles.push_back(Console{
            .nickname = "PS5-Debug-" + std::to_string(i),
            .resting = i % 4 == 0,
            .type = (i % 2 == 0) ? HostType::Auto : HostType::Manual,
        });
    }
    return consoles;
}

}
