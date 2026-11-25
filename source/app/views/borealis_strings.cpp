#include "app/views/borealis_strings.hpp"

#include <borealis/core/i18n.hpp>

namespace akira::app::views {

auto BorealisStrings::text(std::string_view key) const -> std::string
{
    return brls::internal::getRawStr(std::string(key));
}

auto BorealisStrings::pattern(std::string_view key, std::int64_t count) const -> std::string
{
    if (count != 1)
        return text(key);
    const std::string single = std::string(key) + "_one";
    std::string found = text(single);
    return found == single ? text(key) : found;
}

}
