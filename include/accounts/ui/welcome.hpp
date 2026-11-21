#pragma once

#include <cstddef>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "accounts/ui/account_model.hpp"
#include "app/strings.hpp"

namespace akira::accounts::ui {

class Welcome final : public ::arisa::composition::Component {
public:
    static constexpr float tileSide = 108.0F;
    static constexpr float labelSize = 12.5F;
    static constexpr std::size_t labelLines = 2;
    static constexpr float logoHeight = 84.0F;
    static constexpr float mascotSide = 88.0F;
    static constexpr float titleSize = 28.0F;
    static constexpr float titleTracking = -0.7F;
    static constexpr float detailSize = 15.0F;

    Welcome(AccountModel& model, const app::Strings& strings);

    bool operator==(const Welcome& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    AccountModel* model;
    const app::Strings* strings;
};

}
