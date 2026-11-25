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

std::vector<ConsoleFacts> DebugConsoles::facts()
{
    std::vector<ConsoleFacts> shown;
    for (const Console& console : list()) {
        shown.push_back(ConsoleFacts{
            .catalogName = console.nickname,
            .name = console.nickname,
            .type = console.type,
            .paired = true,
            .discovered = true,
            .ps5 = true,
            .standby = console.resting,
            .ready = !console.resting,
            .presence = console.resting ? "Standby" : "Ready",
        });
    }
    return shown;
}

DebugConsoles::DebugConsoles(const Make& make)
{
    for (const Console& console : list())
        owned.push_back(make(console));
}

}
