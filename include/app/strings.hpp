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
        const std::string pattern = text(key);
        try
        {
            return std::vformat(pattern, std::make_format_args(args...));
        }
        catch (const std::format_error& failure)
        {
            log::error("Invalid format \"{}\" from string \"{}\": {}", pattern, key, failure.what());
            return std::string(key);
        }
    }
};

}
