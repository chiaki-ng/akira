#pragma once

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

private:
    auto interpolate(std::string_view key, const std::string& pattern, std::format_args args) const -> std::string;
};

}
