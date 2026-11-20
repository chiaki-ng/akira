#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "play/rumble_profile.hpp"

namespace akira::settings {

class PadBench {
public:
    enum class Summary { Basic, Native, SwitchProfile, DefaultProfile, ThisController, Other };

    struct Pad {
        std::string label;
        std::string key;
        Summary summary = Summary::Other;
        bool direct = false;
        bool tunesMotors = false;
        bool outputReleased = false;
        bool drivesLightbar = false;
        bool joyCon = false;

        friend bool operator==(const Pad&, const Pad&) = default;
    };

    virtual ~PadBench() = default;

    virtual auto scan() -> std::vector<Pad> = 0;
    virtual void use(std::size_t index, const play::RumbleProfile& profile) = 0;
    virtual void refreshOutputGates(std::size_t index) = 0;
    virtual void buzz(std::size_t index, const play::RumbleProfile& profile) = 0;
};

}
