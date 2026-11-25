#pragma once

namespace akira::platform {

class PadRumble {
public:
    struct Wave {
        float freqLow;
        float freqHigh;
        float ampLow;
        float ampHigh;
    };

    virtual ~PadRumble() = default;

    virtual void playerOne(Wave wave) = 0;
    virtual void npad(unsigned npad, Wave wave) = 0;
    virtual void refresh(unsigned npad) = 0;
};

}
