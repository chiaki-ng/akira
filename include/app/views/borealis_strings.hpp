#pragma once

#include "app/strings.hpp"

namespace akira::app::views {

class BorealisStrings : public Strings {
public:
    auto text(std::string_view key) const -> std::string override;
};

}
