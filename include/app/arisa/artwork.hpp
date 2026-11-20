#pragma once

#include <string>

#include <arisa/components/async_image_phase.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

namespace akira::app::arisa {

class Artwork final : public ::arisa::composition::Component {
public:
    Artwork(std::string url, std::string symbol);

    bool operator==(const Artwork& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    static auto shown(const ::arisa::components::AsyncImagePhase& phase, const std::string& symbol)
        -> ::arisa::composition::Element;
    static auto blank(const std::string& symbol) -> ::arisa::composition::Element;

    std::string url;
    std::string symbol;
};

}
