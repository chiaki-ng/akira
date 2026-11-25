#pragma once

#include <libchiaking/trophies_types.hpp>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

namespace akira::trophies::ui {

class GradeCounts final : public ::arisa::composition::Component {
public:
    enum class Size { Compact, Large };

    static constexpr float compactIcon = 16.0F;
    static constexpr float largeIcon = 22.0F;
    static constexpr float compactText = 14.0F;
    static constexpr float largeText = 19.0F;

    GradeCounts(libchiaking::trophies::TrophyCounts counts, Size size);

    bool operator==(const GradeCounts& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    libchiaking::trophies::TrophyCounts counts;
    Size size;
};

}
