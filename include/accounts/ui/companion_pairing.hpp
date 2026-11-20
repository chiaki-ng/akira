#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "accounts/ui/account_model.hpp"
#include "app/strings.hpp"

namespace akira::accounts::ui {

class CompanionPairing final : public ::arisa::composition::Component {
public:
    static constexpr float digitWidth = 68.0F;
    static constexpr float digitHeight = 84.0F;
    static constexpr float width = 640.0F;

    CompanionPairing(AccountModel& model, const app::Strings& strings);

    bool operator==(const CompanionPairing& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto failed(AccountDesk::PairStep step) -> bool;

private:
    static auto digits(const std::string& code) -> ::arisa::composition::Element;

    AccountModel* model;
    const app::Strings* strings;
};

}
