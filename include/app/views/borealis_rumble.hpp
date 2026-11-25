#pragma once

#include "platform/pads/rumble.hpp"

namespace akira::app::views {

class BorealisRumble : public platform::PadRumble {
public:
    void playerOne(Wave wave) override;
    void npad(unsigned npad, Wave wave) override;
    void refresh(unsigned npad) override;
};

}
