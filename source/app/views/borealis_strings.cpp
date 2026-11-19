#include "app/views/borealis_strings.hpp"

#include <borealis/core/i18n.hpp>

namespace akira::app::views {

auto BorealisStrings::text(std::string_view key) const -> std::string
{
    return brls::internal::getRawStr(std::string(key));
}

}
