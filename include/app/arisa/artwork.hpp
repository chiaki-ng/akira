#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <arisa/components/async_image_phase.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/provider/gpu/image.hpp>

namespace akira::app::arisa {

class Artwork final : public ::arisa::composition::Component {
public:
    enum class Scaling { Fill, Fit, Trimmed };

    static constexpr std::uint8_t visibleAlpha = 128;
    static constexpr std::size_t trimmedKept = 24;

    Artwork(std::string url, std::string symbol, Scaling scaling = Scaling::Fill);

    bool operator==(const Artwork& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    static auto shown(const ::arisa::components::AsyncImagePhase& phase, const std::string& symbol, Scaling scaling)
        -> ::arisa::composition::Element;
    static auto blank(const std::string& symbol) -> ::arisa::composition::Element;
    static auto trimmed(const std::shared_ptr<const ::arisa::provider::Image>& source)
        -> std::shared_ptr<const ::arisa::provider::Image>;
    static auto trim(const ::arisa::provider::Image& source) -> std::shared_ptr<const ::arisa::provider::Image>;

    std::string url;
    std::string symbol;
    Scaling scaling;
};

}
