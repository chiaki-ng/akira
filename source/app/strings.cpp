#include "app/strings.hpp"

#include "log/log.hpp"

namespace akira::app {

auto Strings::interpolate(std::string_view key, const std::string& pattern, std::format_args args) const -> std::string
{
    try
    {
        return std::vformat(pattern, args);
    }
    catch (const std::format_error& failure)
    {
        log::error("Invalid format \"{}\" from string \"{}\": {}", pattern, key, failure.what());
        return std::string(key);
    }
}

}
