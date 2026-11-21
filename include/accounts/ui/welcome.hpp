#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "accounts/ui/account_model.hpp"
#include "app/strings.hpp"

namespace akira::accounts::ui {

class Welcome final : public ::arisa::composition::Component {
public:
    static constexpr float tileSide = 132.0F;
    static constexpr float hintWidth = 600.0F;

    Welcome(AccountModel& model, const app::Strings& strings);

    bool operator==(const Welcome& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    AccountModel* model;
    const app::Strings* strings;
};

}
