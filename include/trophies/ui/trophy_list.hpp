#pragma once

#include <optional>
#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/theme/glass.hpp>

#include "app/strings.hpp"
#include "trophies/ui/trophy_model.hpp"

namespace akira::trophies::ui {

class TrophyList final : public ::arisa::composition::Component {
public:
    static constexpr float iconSize = 64.0F;
    static constexpr float iconRadius = 10.0F;
    static constexpr float barWidth = 96.0F;
    static constexpr float summaryBarWidth = 220.0F;
    static constexpr float rowPadding = 10.0F;
    static constexpr float listPadding = 6.0F;
    static constexpr float spacing = 16.0F;

    TrophyList(TrophyModel& model, const app::Strings& strings);

    bool operator==(const TrophyList& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto dated(const std::optional<libchiaking::trophies::Time>& at) -> std::string;

private:
    auto toolbar(const TrophyModel::View& view) const -> ::arisa::composition::Element;
    auto summary(const TrophyModel::View& view) const -> ::arisa::composition::Element;
    auto rows(const TrophyModel::View& view, ::arisa::theme::Glass glass) const -> ::arisa::composition::Element;
    auto sortMenu(const TrophyModel::View& view) const -> ::arisa::composition::Element;
    auto filterMenu(const TrophyModel::View& view) const -> ::arisa::composition::Element;
    auto refreshButton(const TrophyModel::View& view) const -> ::arisa::composition::Element;
    static auto row(const TrophyModel::Row& shown, TrophyModel* model) -> ::arisa::composition::Element;
    static auto platforms(const TrophyModel::Title& title) -> ::arisa::composition::Element;

    TrophyModel* model;
    const app::Strings* strings;
};

}
