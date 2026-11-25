#pragma once

#include <arisa/async/task.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "accounts/ui/account_model.hpp"
#include "app/strings.hpp"

namespace akira::accounts::ui {

class AccountFlow final : public ::arisa::composition::Component {
public:
    static constexpr float glowOpacity = 0.10F;

    AccountFlow(AccountModel& model, const app::Strings& strings);

    bool operator==(const AccountFlow& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    static auto backdrop() -> ::arisa::composition::Element;
    static auto ticking(AccountModel* model) -> ::arisa::async::Task<void>;

    AccountModel* model;
    const app::Strings* strings;
};

}
