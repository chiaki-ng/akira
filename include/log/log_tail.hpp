#pragma once

#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace akira::log {

class LogTail {
public:
    enum class Stamp { None, Clock };

    LogTail(std::size_t capacity, Stamp stamp);

    void mask(std::string needle, std::string replacement);
    void push(std::string_view line);
    std::vector<std::string> lines() const;

private:
    static void replaceAll(std::string& text, const std::string& needle, const std::string& replacement);
    static std::string clock();

    std::size_t capacity;
    Stamp stamp;
    std::vector<std::pair<std::string, std::string>> masks;
    std::deque<std::string> kept;
    mutable std::mutex mutex;
};

}
