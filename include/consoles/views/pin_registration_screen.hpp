#pragma once

#include <string>

#include "consoles/register_flow.hpp"

namespace akira::consoles {
class Host;
}

namespace akira::consoles::views {

class PinRegistrationScreen {
public:
    static void open(Host* host);

private:
    static std::string refusalText(RegisterFlow::Refusal refusal);
};

}
