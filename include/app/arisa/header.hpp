#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

namespace akira::app::arisa {

class Header final : public ::arisa::composition::Component {
public:
    static constexpr float spacing = 14.0F;

    explicit Header(std::string title);

    bool operator==(const Header& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    std::string title;
};

}
