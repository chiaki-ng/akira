#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>

#include "accounts/ui/account_model.hpp"
#include "app/strings.hpp"

namespace akira::accounts::ui {

class LegacyProfile final : public ::arisa::composition::Component {
public:
    static constexpr float width = 600.0F;
    static const ::arisa::composition::StateKey<std::string> accountId;
    static const ::arisa::composition::StateKey<std::string> name;

    LegacyProfile(AccountModel& model, const app::Strings& strings);

    bool operator==(const LegacyProfile& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    AccountModel* model;
    const app::Strings* strings;
};

}
