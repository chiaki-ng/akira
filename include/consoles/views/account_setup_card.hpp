#pragma once

#include <borealis.hpp>

namespace akira::consoles::views {

class AccountSetupCard : public brls::Box {
public:
    AccountSetupCard();

private:
    brls::Animatable focusAnim{0.0f};
};

}
