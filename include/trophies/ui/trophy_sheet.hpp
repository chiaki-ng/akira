#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "trophies/ui/trophy_model.hpp"

namespace akira::trophies::ui {

class TrophySheet final : public ::arisa::composition::Component {
public:
    static constexpr float iconSize = 128.0F;
    static constexpr float iconRadius = 16.0F;
    static constexpr float width = 520.0F;
    static constexpr float barWidth = 320.0F;

    TrophySheet(TrophyModel::Trophy trophy, TrophyModel& model, const app::Strings& strings);

    bool operator==(const TrophySheet& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    TrophyModel::Trophy trophy;
    TrophyModel* model;
    const app::Strings* strings;
};

}
