#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <string>

#include <libchiaking/platform/platform.hpp>

#include "app/strings.hpp"

namespace akira::app {

class GhashBenchmark {
public:
    using Line = std::function<void(std::string)>;

    static constexpr int iterations = 10000;
    static constexpr std::array<std::size_t, 4> sizes{ 64, 256, 1024, 4096 };

    GhashBenchmark(libchiaking::platform::CryptoProvider& crypto, const libchiaking::platform::ClockProvider& clock,
        const Strings& strings);

    void run(const libchiaking::platform::StopToken& stop, const Line& line) const;

private:
    auto measure(std::size_t size, const libchiaking::platform::StopToken& stop, const Line& line) const -> bool;

    libchiaking::platform::CryptoProvider& crypto;
    const libchiaking::platform::ClockProvider& clock;
    const Strings& strings;
};

}
