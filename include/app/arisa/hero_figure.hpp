#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <arisa/components/async_image_phase.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/provider/gpu/image.hpp>

namespace akira::app::arisa {

class HeroFigure final : public ::arisa::composition::Component {
public:
    struct Placement {
        float centre;
        float roomyWidth;
        float spareShift;
        float figureHeight;
        float figureMiddle;

        friend bool operator==(const Placement&, const Placement&) = default;
    };

    struct Bounds {
        float x = 0.0F;
        float y = 0.0F;
        float width = 1.0F;
        float height = 1.0F;

        friend bool operator==(const Bounds&, const Bounds&) = default;
    };

    static constexpr std::uint32_t sample = 96;
    static constexpr std::uint64_t trimPercent = 5;
    static constexpr float thinnest = 0.05F;
    static constexpr std::size_t boundsKept = 24;

    HeroFigure(std::string url, Placement placement);

    bool operator==(const HeroFigure& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto measured(const ::arisa::provider::Image& image) -> Bounds;

private:
    static auto placed(const ::arisa::components::AsyncImagePhase& phase, ::arisa::composition::Size area, Placement placement)
        -> ::arisa::composition::Element;
    static auto bounds(const std::shared_ptr<const ::arisa::provider::Image>& image) -> Bounds;
    static auto bulk(const std::vector<std::uint64_t>& weights) -> std::optional<std::pair<std::size_t, std::size_t>>;

    std::string url;
    Placement placement;
};

}
