#pragma once

#include <functional>
#include <string>

#include <arisa/composition/element.hpp>

#include "app/strings.hpp"

namespace akira::app::arisa {

class LinkCode {
public:
    static constexpr float side = 260.0F;
    static constexpr float spacing = 16.0F;
    static constexpr float padding = 24.0F;
    static constexpr float scrim = 0.32F;

    LinkCode(std::string url, std::function<void()> closed);

    auto card(const app::Strings& strings) const -> ::arisa::composition::Element;
    auto dimmed(const app::Strings& strings) const -> ::arisa::composition::Element;

private:
    std::string url;
    std::function<void()> closed;
};

}
