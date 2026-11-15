#pragma once

#include <cstdint>

namespace akira::platform
{

enum class PlatformCapability : std::uint32_t {
    none = 0,
    couch_seats = 1u << 0,
    self_update = 1u << 1,
    stats_export = 1u << 2,
    hevc = 1u << 3,
    fsr_4k = 1u << 4,
    zero_copy = 1u << 5,
};

constexpr PlatformCapability operator|(PlatformCapability left, PlatformCapability right) noexcept
{
    return static_cast<PlatformCapability>(static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

constexpr PlatformCapability operator&(PlatformCapability left, PlatformCapability right) noexcept
{
    return static_cast<PlatformCapability>(static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
}

struct PlatformInfo {
    const char* name;
    PlatformCapability capabilities;
};

}
