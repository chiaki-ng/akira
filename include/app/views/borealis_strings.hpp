#pragma once

#include "app/strings.hpp"

namespace akira::app::views {

class BorealisStrings : public Strings {
public:
    auto text(std::string_view key) const -> std::string override;

protected:
    auto pattern(std::string_view key, std::int64_t count) const -> std::string override;
};

}
