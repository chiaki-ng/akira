#include "log/log_tail.hpp"

#include <chrono>
#include <ctime>
#include <format>

namespace akira::log {

LogTail::LogTail(std::size_t capacity, Stamp stamp)
    : capacity(capacity)
    , stamp(stamp)
{
}

void LogTail::mask(std::string needle, std::string replacement)
{
    std::lock_guard<std::mutex> lock(mutex);
    masks.emplace_back(std::move(needle), std::move(replacement));
}

void LogTail::push(std::string_view line)
{
    std::lock_guard<std::mutex> lock(mutex);
    std::string safeLine(line);
    for (const auto& [needle, replacement] : masks)
        replaceAll(safeLine, needle, replacement);
    kept.push_back(stamp == Stamp::Clock ? clock() + " " + safeLine : std::move(safeLine));
    while (kept.size() > capacity)
        kept.pop_front();
}

std::vector<std::string> LogTail::lines() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return {kept.begin(), kept.end()};
}

void LogTail::replaceAll(std::string& text, const std::string& needle, const std::string& replacement)
{
    if (needle.empty() || needle == replacement)
        return;
    size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos)
    {
        text.replace(position, needle.size(), replacement);
        position += replacement.size();
    }
}

std::string LogTail::clock()
{
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()).count() % 1000;
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm time_tm = *std::localtime(&tt);
    return std::format("{:02}:{:02}:{:02}.{:03}", time_tm.tm_hour, time_tm.tm_min, time_tm.tm_sec,
                       static_cast<int>(ms));
}

}
