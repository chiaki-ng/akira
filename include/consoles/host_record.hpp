#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <libchiaking/consoles_types.hpp>

#include "consoles/host_type.hpp"
#include "consoles/registration.hpp"

namespace akira::consoles {

struct HostRecord {
    std::string nickname;
    std::string serverNickname;
    int64_t consoleId = 0;
    HostType hostType = HostType::Discovered;
    std::string hostAddr;
    std::optional<libchiaking::consoles::Firmware> firmware;
    std::optional<libchiaking::consoles::HardwareId> hardwareId;
    std::string consolePin;
    int haptic = -1;
    std::optional<std::uint32_t> pinnedTakionVersion;
    std::string remoteDuid;
    std::vector<Registration> legacyRegistrations;
};

}
