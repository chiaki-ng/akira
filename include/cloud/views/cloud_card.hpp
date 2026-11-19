#pragma once

#include <borealis.hpp>

namespace akira::cloud::views {

class CloudCard : public brls::Box {
public:
    CloudCard();

private:
    brls::Animatable focusAnim{0.0f};
};

}
