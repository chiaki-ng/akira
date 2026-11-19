#include "log/core_log.hpp"
#include "settings/settings_manager.hpp"
#include "log/log.hpp"

namespace akira {

using libchiaking::platform::LogRecord;
using libchiaking::platform::LogSeverity;

CoreLog::CoreLog() noexcept
    : mask(maskFor(ChiakiLogVerbosity::Normal, false))
{
}

void CoreLog::write(LogRecord record) noexcept
{
    if ((mask.load(std::memory_order_relaxed) & bit(record.severity)) == 0)
        return;

    try {
        switch (record.severity) {
            case LogSeverity::error:
                akira::log::error("{}", record.message);
                break;
            case LogSeverity::warning:
                akira::log::warning("{}", record.message);
                break;
            case LogSeverity::info:
                akira::log::info("{}", record.message);
                break;
            case LogSeverity::debug:
                akira::log::debug("{}", record.message);
                break;
            case LogSeverity::verbose:
                akira::log::verbose("{}", record.message);
                break;
        }
    } catch (...) {
    }
}

void CoreLog::follow(ChiakiLogVerbosity verbosity, bool streaming, bool silenced) noexcept
{
    mask.store(silenced ? std::uint8_t{0} : maskFor(verbosity, streaming), std::memory_order_relaxed);
}

auto CoreLog::maskFor(ChiakiLogVerbosity verbosity, bool streaming) noexcept -> std::uint8_t
{
    constexpr bool muted = AKIRA_MUTE_CORE_LOGS != 0;
    constexpr std::uint8_t warnings = bit(LogSeverity::error) | bit(LogSeverity::warning);
    constexpr std::uint8_t info = warnings | bit(LogSeverity::info);
    constexpr std::uint8_t debug = info | bit(LogSeverity::debug);
    constexpr std::uint8_t everything = debug | bit(LogSeverity::verbose);

    if (muted)
        return 0;

    switch (verbosity) {
        case ChiakiLogVerbosity::Info:
            return info;
        case ChiakiLogVerbosity::Debug:
            return debug;
        case ChiakiLogVerbosity::Trace:
            return everything;
        case ChiakiLogVerbosity::Normal:
            break;
    }
    return streaming ? warnings : info;
}

}
