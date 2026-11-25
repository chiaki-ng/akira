#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "cloud/ui/cloud_model.hpp"

namespace akira::cloud::ui {

class ShortcutsRail final : public ::arisa::composition::Component {
public:
    static constexpr std::size_t shown = 6;
    static constexpr float tileWidth = 132.0F;
    static constexpr float tileRadius = 10.0F;

    ShortcutsRail(CloudModel& model, const app::Strings& strings);

    bool operator==(const ShortcutsRail& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    static auto tile(const Game& game, CloudModel* model, const app::Strings* strings) -> ::arisa::composition::Element;

    CloudModel* model;
    const app::Strings* strings;
};

}
