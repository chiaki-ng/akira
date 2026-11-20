#pragma once

#include <functional>
#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/theme/glass.hpp>

#include "app/strings.hpp"
#include "trophies/ui/trophy_model.hpp"

namespace akira::trophies::ui {

class TrophyPage final : public ::arisa::composition::Component {
public:
    static constexpr float artSize = 48.0F;
    static constexpr float artRadius = 8.0F;
    static constexpr float iconSize = 56.0F;
    static constexpr float iconRadius = 10.0F;
    static constexpr float barWidth = 200.0F;
    static constexpr float gradeWidth = 110.0F;
    static constexpr float spacing = 14.0F;

    TrophyPage(TrophyModel& model, const app::Strings& strings);

    bool operator==(const TrophyPage& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto heading(const TrophyModel::Page& page) const -> ::arisa::composition::Element;
    auto groupChips(const TrophyModel::View& view) const -> ::arisa::composition::Element;
    auto choiceChips(const TrophyModel::View& view) const -> ::arisa::composition::Element;
    auto list(const TrophyModel::View& view, ::arisa::theme::Glass glass) const -> ::arisa::composition::Element;
    auto row(const TrophyModel::Trophy& trophy, bool revealed) const -> ::arisa::composition::Element;
    auto icon(const TrophyModel::Trophy& trophy, bool secret) const -> ::arisa::composition::Element;
    static auto chip(std::string label, bool chosen, std::function<void()> pressed, std::string key)
        -> ::arisa::composition::Element;

    TrophyModel* model;
    const app::Strings* strings;
};

}
