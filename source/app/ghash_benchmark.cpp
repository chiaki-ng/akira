#include "app/ghash_benchmark.hpp"

#include <format>
#include <vector>

namespace akira::app {

GhashBenchmark::GhashBenchmark(libchiaking::platform::CryptoProvider& crypto,
    const libchiaking::platform::ClockProvider& clock, const Strings& strings)
    : crypto(crypto)
    , clock(clock)
    , strings(strings)
{
}

auto GhashBenchmark::measure(std::size_t size, const libchiaking::platform::StopToken& stop, const Line& line) const -> bool
{
    static constexpr std::array<std::byte, 16> key{ std::byte{ 0x01 }, std::byte{ 0x02 }, std::byte{ 0x03 }, std::byte{ 0x04 },
        std::byte{ 0x05 }, std::byte{ 0x06 }, std::byte{ 0x07 }, std::byte{ 0x08 }, std::byte{ 0x09 }, std::byte{ 0x0a },
        std::byte{ 0x0b }, std::byte{ 0x0c }, std::byte{ 0x0d }, std::byte{ 0x0e }, std::byte{ 0x0f }, std::byte{ 0x10 } };
    static constexpr std::array<std::byte, 12> iv{};
    const std::vector<std::byte> data(size, std::byte{ 0xAB });
    std::array<std::byte, 16> tag{};
    line(strings.text("akira/benchmark/separator"));
    line(strings.format("akira/benchmark/testing_blocks", size));
    line(std::string());
    line(strings.format("akira/benchmark/pmull_running", iterations));
    const std::uint64_t started = clock.now_microseconds();
    int completed = 0;
    for (; completed < iterations && !stop.stop_requested(); ++completed)
    {
        if (!crypto.aes128_gmac(key, iv, data, tag))
        {
            line(strings.text("akira/benchmark/error_gmac"));
            break;
        }
    }
    const double elapsedMs = static_cast<double>(clock.now_microseconds() - started) / 1000.0;
    if (completed == 0 || elapsedMs <= 0.0)
        return false;
    const double operations = completed * 1000.0 / elapsedMs;
    const double megabytes = completed * static_cast<double>(size) * 1000.0 / (elapsedMs * 1024.0 * 1024.0);
    line(strings.format("akira/benchmark/pmull_result", std::format("{:.0f}", operations), std::format("{:.2f}", megabytes),
        std::format("{:.1f}", elapsedMs)));
    line(std::string());
    return true;
}

void GhashBenchmark::run(const libchiaking::platform::StopToken& stop, const Line& line) const
{
    line(strings.text("akira/benchmark/header_line"));
    line(strings.text("akira/benchmark/results_title"));
    line(strings.text("akira/benchmark/header_line"));
    line(std::string());
    line(strings.format("akira/benchmark/iterations", iterations));
    line(std::string());
    for (const std::size_t size : sizes)
        if (stop.stop_requested() || !measure(size, stop, line))
            break;
    line(strings.text("akira/benchmark/header_line"));
    line(strings.text("akira/benchmark/complete_title"));
    line(strings.text("akira/benchmark/header_line"));
}

}
