#pragma once

#include <concepts>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

#include "log/log.hpp"

namespace akira::app {

class Strings {
public:
    virtual ~Strings() = default;

    virtual auto text(std::string_view key) const -> std::string = 0;

    template <typename... Args>
    auto format(std::string_view key, const Args&... args) const -> std::string
    {
        return interpolate(key, text(key), std::make_format_args(args...));
    }

    template <std::integral Count, typename... Args>
    auto counted(std::string_view key, const Count& count, const Args&... args) const -> std::string
    {
        return interpolate(key, pattern(key, static_cast<std::int64_t>(count)), std::make_format_args(count, args...));
    }

protected:
    virtual auto pattern(std::string_view key, std::int64_t count) const -> std::string;

private:
    auto interpolate(std::string_view key, const std::string& written, std::format_args args) const -> std::string;
};

}
