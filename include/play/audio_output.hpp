#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>

namespace akira::play {

class AudioOutput {
public:
    virtual ~AudioOutput() = default;

    [[nodiscard]] virtual auto open(unsigned channels, unsigned rate) -> std::expected<void, std::string> = 0;
    virtual void play(std::span<const std::int16_t> samples) = 0;
    virtual void close() = 0;
};

}
