#include "app/strings.hpp"

#include "log/log.hpp"

namespace akira::app {

auto Strings::pattern(std::string_view key, std::int64_t) const -> std::string
{
    return text(key);
}

auto Strings::interpolate(std::string_view key, const std::string& written, std::format_args args) const -> std::string
{
    try
    {
        return std::vformat(written, args);
    }
    catch (const std::format_error& failure)
    {
        log::error("Invalid format \"{}\" from string \"{}\": {}", written, key, failure.what());
        return std::string(key);
    }
}

}
