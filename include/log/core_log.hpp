#pragma once

#include <atomic>
#include <cstdint>

#include <libchiaking/platform/platform.hpp>

namespace akira::settings {
enum class ChiakiLogVerbosity;
}

namespace akira {

class CoreLog final : public libchiaking::platform::DiagnosticsProvider {
public:
    CoreLog() noexcept;

    void write(libchiaking::platform::LogRecord record) noexcept override;
    void follow(settings::ChiakiLogVerbosity verbosity, bool streaming, bool silenced) noexcept;

private:
    static constexpr auto bit(libchiaking::platform::LogSeverity severity) noexcept -> std::uint8_t
    {
        return static_cast<std::uint8_t>(1u << static_cast<unsigned>(severity));
    }

    static auto maskFor(settings::ChiakiLogVerbosity verbosity, bool streaming) noexcept -> std::uint8_t;

    std::atomic<std::uint8_t> mask;
};

}
