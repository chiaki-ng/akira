#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "accounts/ui/account_model.hpp"
#include "app/strings.hpp"

namespace akira::accounts::ui {

class QrSignIn final : public ::arisa::composition::Component {
public:
    static constexpr float codeSide = 240.0F;
    static constexpr float width = 680.0F;

    QrSignIn(AccountModel& model, const app::Strings& strings);

    bool operator==(const QrSignIn& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto code(const AccountModel::Qr& qr) const -> ::arisa::composition::Element;
    auto status(const AccountModel::Qr& qr) const -> ::arisa::composition::Element;

    AccountModel* model;
    const app::Strings* strings;
};

}
