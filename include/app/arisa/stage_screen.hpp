#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/arisa/notices.hpp"

namespace akira::app::arisa {

class StageScreen final : public ::arisa::composition::Component {
public:
    explicit StageScreen(Notices& notices);

    bool operator==(const StageScreen& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    static constexpr float keepAliveSize = 1.0F;
    static constexpr float keepAliveAlpha = 1.0F / 255.0F;

    static auto keepAlive() -> ::arisa::composition::Element;

    Notices* notices;
};

}
