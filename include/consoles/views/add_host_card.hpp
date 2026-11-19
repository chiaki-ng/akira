#pragma once

#include <borealis.hpp>

namespace akira::consoles::views {

class AddHostCard : public brls::Box {
public:
    AddHostCard();

private:
    static void openAddHostFlow();

    brls::Animatable focusAnim{0.0f};
};

}
