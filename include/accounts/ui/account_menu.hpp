#pragma once

#include <functional>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "accounts/ui/account_model.hpp"
#include "app/strings.hpp"

namespace akira::accounts::ui {

class AccountMenu final : public ::arisa::composition::Component {
public:
    static constexpr float avatarSide = 30.0F;
    static constexpr float nameWidth = 180.0F;

    AccountMenu(AccountModel& model, const app::Strings& strings);

    bool operator==(const AccountMenu& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto choices(AccountModel& model, const AccountModel::View& view, const app::Strings& strings)
        -> std::function<::arisa::composition::Element()>;

private:
    AccountModel* model;
    const app::Strings* strings;
};

}
