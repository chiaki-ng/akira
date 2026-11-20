#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "trophies/ui/trophy_model.hpp"

namespace akira::trophies::ui {

class TrophiesScreen final : public ::arisa::composition::Component {
public:
    TrophiesScreen(TrophyModel& model, const app::Strings& strings);

    bool operator==(const TrophiesScreen& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto content(const TrophyModel::View& view) const -> ::arisa::composition::Element;
    auto unavailable(const std::string& title, const std::string& symbol, bool retry) const
        -> ::arisa::composition::Element;
    auto detailShown() const -> ::arisa::composition::Binding<bool>;

    TrophyModel* model;
    const app::Strings* strings;
};

}
