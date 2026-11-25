#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace akira::consoles {

class TakionPinChoice {
public:
    TakionPinChoice(std::span<const std::uint32_t> versions, std::optional<std::uint32_t> pinned, int configured);

    const std::vector<std::uint32_t>& versions() const { return pinnable; }
    std::optional<std::uint32_t> configuredVersion() const;
    int selected() const;
    bool contains(int index) const;
    std::optional<std::uint32_t> versionAt(int index) const;

private:
    std::vector<std::uint32_t> pinnable;
    std::optional<std::uint32_t> pinned;
    int configured;
};

}
