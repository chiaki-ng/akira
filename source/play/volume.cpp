#include "play/volume.hpp"

#include <algorithm>
#include <cmath>

namespace akira::play {

Volume::Volume(double level)
    : chosen(std::isfinite(level) ? std::clamp(std::round(level / step) * step, lowest, highest) : highest)
{
}

}
